#include "acceptor.h"

#include <fcntl.h>
#include <unistd.h>

#include <cassert>

#include <mini_muduo/log.h>
#include <mini_muduo/socket_ops.h>

namespace mini_muduo {

Acceptor::Acceptor(EventLoop *pLoop, const InetAddress &listenAddr, bool reuseport)
    : pOwnerLoop_(pLoop)
    , acceptSocket_(socket_ops::createNonblockingOrDie(listenAddr.family()))
    , acceptChannel_(pLoop, acceptSocket_.fd())
    , idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC)) {
    assert(idleFd_ >= 0);

    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reuseport);
    acceptSocket_.bind(listenAddr);

    acceptChannel_.setReadCallback([this](Timestamp receiveTime) {
        (void)receiveTime;

        this->handleRead();
    });
}

Acceptor::~Acceptor() {
    acceptChannel_.disableAll();
    acceptChannel_.remove();

    ::close(idleFd_);
}

void Acceptor::listen() {
    pOwnerLoop_->assertInLoopThread();

    listening_ = true;

    acceptSocket_.listen();
    acceptChannel_.enableReading();
}

void Acceptor::handleRead() {
    pOwnerLoop_->assertInLoopThread();

    InetAddress peerAddr;

    // FIXME loop until no more
    const int connFd = acceptSocket_.accept(&peerAddr);

    if (connFd >= 0) {
        if (newConnectionCallback_) {
            newConnectionCallback_(connFd, peerAddr);
        } else {
            socket_ops::close(connFd);
        }
    } else {
        MINI_MUDUO_LOG_ERROR("accept()");

        // Read the section named "The special problem of
        // accept()ing when you can't" in libev's doc.
        // By Marc Lehmann, author of libev.
        if (errno == EMFILE) {
            ::close(idleFd_);
            idleFd_ = ::accept(acceptSocket_.fd(), nullptr, nullptr);
            ::close(idleFd_);
            idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
    }
}

}  // namespace mini_muduo
