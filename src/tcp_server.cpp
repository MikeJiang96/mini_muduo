#include <mini_muduo/tcp_server.h>

#include <cassert>
#include <memory>
#include <mutex>
#include <utility>

#include "acceptor.h"

#include <mini_muduo/log.h>
#include <mini_muduo/socket_ops.h>

namespace mini_muduo {

TcpServer::TcpServer(EventLoop *pLoop, const InetAddress &listenAddr, std::string name, int nThreads, Option option)
    : pOwnerMainLoop_(pLoop)
    , name_(std::move(name))
    , ipPort_(listenAddr.toIpPort())
    , threadPool_(pLoop, nThreads)
    , acceptor_(std::make_unique<Acceptor>(pOwnerMainLoop_, listenAddr, option == Option::REUSE_PORT)) {
    acceptor_->setNewConnectionCallback([this](int sockFd, const InetAddress &peerAddr) {
        this->onNewConnection(sockFd, peerAddr);
    });
}

TcpServer::~TcpServer() {
    pOwnerMainLoop_->assertInLoopThread();

    for (auto &item : connections_) {
        TcpConnectionPtr conn(item.second);

        item.second.reset();

        conn->getLoop()->runInLoop([conn = std::move(conn)] {
            conn->onConnectionDestroyed();
        });
    }
}

void TcpServer::start() {
    assert(!acceptor_->listening());

    std::call_once(startOnce_, [this] {
        this->threadPool_.start();

        this->pOwnerMainLoop_->runInLoop([this] {
            this->acceptor_->listen();
        });
    });
}

void TcpServer::onNewConnection(int sockFd, const InetAddress &peerAddr) {
    pOwnerMainLoop_->assertInLoopThread();

    char buf[64];
    snprintf(buf, sizeof(buf), "-%s#%d", ipPort_.c_str(), nextConnId_++);

    std::string connName = name_ + buf;

    MINI_MUDUO_LOG_INFO(
        "TcpServer::newConnection [{}] - new connection [{}] from {}", name_, connName, peerAddr.toIpPort());

    InetAddress localAddr(socket_ops::getLocalAddr(sockFd));

    // FIXME poll with zero timeout to double confirm the new connection
    EventLoop *pIoLoop = threadPool_.getNextLoop();

    const auto conn = std::make_shared<TcpConnection>(pIoLoop, connName, sockFd, localAddr, peerAddr);

    connections_[connName] = conn;

    conn->setConnectionCallback(connectionCb_);
    conn->setMessageCallback(messageCb_);
    conn->setWriteCompleteCallback(writeCompleteCb_);

    // DO NOT capture conn to avoid mutual reference TcpConnection class
    conn->setCloseCallback([this](const TcpConnectionPtr &argConn) {
        this->removeConnection(argConn);
    });  // FIXME: unsafe. Why???

    pIoLoop->runInLoop([conn] {
        conn->onConnectionEstablished();
    });
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn) {
    // FIXME: unsafe. Why???
    pOwnerMainLoop_->runInLoop([this, conn] {
        this->removeConnectionInLoop(conn);
    });
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn) {
    pOwnerMainLoop_->assertInLoopThread();

    MINI_MUDUO_LOG_INFO("TcpServer::removeConnectionInLoop [{}] - connection {}", name_, conn->name());

    const size_t n = connections_.erase(conn->name());
    (void)n;
    assert(n == 1);

    EventLoop *pIoLoop = conn->getLoop();

    pIoLoop->queueInLoop([conn] {
        conn->onConnectionDestroyed();
    });
}

}  // namespace mini_muduo
