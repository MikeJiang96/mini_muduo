#include <mini_muduo/tcp_client.h>

#include <cassert>
#include <chrono>

#include "connector.h"

#include <mini_muduo/inet_address.h>
#include <mini_muduo/log.h>
#include <mini_muduo/socket_ops.h>

namespace mini_muduo {

TcpClient::TcpClient(EventLoop *pLoop, const InetAddress &serverAddr, std::string name)
    : pOwnerLoop_(pLoop)
    , name_(std::move(name))
    , connector_(std::make_shared<Connector>(pLoop, serverAddr)) {
    connector_->setNewConnectionCallback([this](int sockFd) {
        onNewConnection(sockFd);
    });

    MINI_MUDUO_LOG_INFO("TcpClient::TcpClient[{}]", name_);
}

TcpClient::~TcpClient() {
    MINI_MUDUO_LOG_INFO("TcpClient::~TcpClient[{}]", name_);

    TcpConnectionPtr conn;
    bool unique = false;

    {
        std::lock_guard lg{mu_};
        unique = conn_.unique();
        conn = conn_;
    }

    if (conn) {
        assert(pOwnerLoop_ == conn->getLoop());

        // FIXME: not 100% safe, if we are in different thread
        auto newCloseCb = [pLoop = pOwnerLoop_](const TcpConnectionPtr &argConn) {
            pLoop->queueInLoop([argConn] {
                MINI_MUDUO_LOG_DEBUG("Destroy connection when TcpClient was already destroyed");

                argConn->onConnectionDestroyed();
            });
        };

        pOwnerLoop_->runInLoop([conn, cb = std::move(newCloseCb)] {
            conn->setCloseCallback(cb);
        });

        if (unique) {
            conn->forceClose();
        }
    } else {
        connector_->stop();

        // FIXME: HACK
        pOwnerLoop_->runAfter(std::chrono::seconds(1), [connector = this->connector_] {
            (void)connector;
        });
    }
}

void TcpClient::connect() {
    MINI_MUDUO_LOG_INFO("TcpClient::connect[{}] - connecting to {}", name_, connector_->serverAddress().toIpPort());

    connect_.store(true);
    connector_->start();
}

void TcpClient::disconnect() {
    connect_.store(false);

    {
        std::lock_guard lg{mu_};

        if (conn_) {
            conn_->shutdown();
        }
    }
}

void TcpClient::stop() {
    connect_.store(false);

    connector_->stop();
}

void TcpClient::onNewConnection(int sockFd) {
    pOwnerLoop_->assertInLoopThread();

    const InetAddress peerAddr(socket_ops::getPeerAddr(sockFd));
    char buf[32];
    snprintf(buf, sizeof buf, ":%s#%d", peerAddr.toIpPort().c_str(), nextConnId_++);

    const std::string connName = name_ + buf;

    const InetAddress localAddr(socket_ops::getLocalAddr(sockFd));

    // FIXME poll with zero timeout to double confirm the new connection
    const auto conn = std::make_shared<TcpConnection>(pOwnerLoop_, connName, sockFd, localAddr, peerAddr);

    conn->setConnectionCallback(connectionCb_);
    conn->setMessageCallback(messageCb_);
    conn->setWriteCompleteCallback(writeCompleteCb_);

    conn->setCloseCallback([this](const TcpConnectionPtr &argConn) {
        this->removeConnection(argConn);
    });  // FIXME: unsafe. Why???

    {
        std::lock_guard lg{mu_};
        conn_ = conn;
    }

    conn->onConnectionEstablished();
}

void TcpClient::removeConnection(const TcpConnectionPtr &conn) {
    pOwnerLoop_->assertInLoopThread();
    assert(pOwnerLoop_ == conn->getLoop());

    {
        std::lock_guard lg{mu_};
        assert(conn_ == conn);

        conn_.reset();
    }

    pOwnerLoop_->queueInLoop([conn]() {
        conn->onConnectionDestroyed();
    });

    if (retry_ && connect_) {
        MINI_MUDUO_LOG_INFO(
            "TcpClient::connect[{}] - Reconnecting to {}", name_, connector_->serverAddress().toIpPort());

        connector_->restart();
    }
}

}  // namespace mini_muduo
