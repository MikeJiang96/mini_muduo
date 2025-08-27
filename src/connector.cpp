#include "connector.h"

#include <cassert>
#include <cerrno>

#include <mini_muduo/log.h>
#include <mini_muduo/socket_ops.h>

namespace mini_muduo {

void Connector::start() {
    connect_.store(true);

    pOwnerLoop_->runInLoop([this] {  // FIXME: unsafe. Why???
        this->startInLoop();
    });
}

void Connector::startInLoop() {
    pOwnerLoop_->assertInLoopThread();

    assert(state_ == State::DISCONNECTED);

    if (connect_.load()) {
        connect();
    } else {
        MINI_MUDUO_LOG_DEBUG("do not connect");
    }
}

void Connector::connect() {
    const int sockFd = socket_ops::createNonblockingOrDie(serverAddr_.family());
    const int connnectRet = socket_ops::connect(sockFd, serverAddr_.getSockAddr());
    const int savedErrno = (connnectRet == 0) ? 0 : errno;

    switch (savedErrno) {
        case 0:
        case EINPROGRESS:
        case EINTR:
        case EISCONN:
            connecting(sockFd);
            break;

        case EAGAIN:
        case EADDRINUSE:
        case EADDRNOTAVAIL:
        case ECONNREFUSED:
        case ENETUNREACH:
            retry(sockFd);
            break;

        case EACCES:
        case EPERM:
        case EAFNOSUPPORT:
        case EALREADY:
        case EBADF:
        case EFAULT:
        case ENOTSOCK:
            MINI_MUDUO_LOG_ERROR("connect error in Connector::startInLoop {} {}", savedErrno, strerror_tl(savedErrno));
            socket_ops::close(sockFd);
            break;

        default:
            MINI_MUDUO_LOG_ERROR(
                "Unexpected error in Connector::startInLoop {} {}", savedErrno, strerror_tl(savedErrno));
            socket_ops::close(sockFd);
            // connectErrorCallback_();
            break;
    }
}

void Connector::connecting(int sockFd) {
    setState(State::CONNECTING);

    assert(!channel_);

    channel_ = std::make_unique<Channel>(pOwnerLoop_, sockFd);

    channel_->setWriteCallback([this] {  // FIXME: unsafe. Why???
        this->handleWrite();
    });

    channel_->setErrorCallback([this] {  // FIXME: unsafe. Why???
        this->handleError();
    });

    // channel_->tie(shared_from_this()); is not working,
    // as channel_ is not managed by shared_ptr
    channel_->enableWriting();
}

void Connector::retry(int sockFd) {
    socket_ops::close(sockFd);

    setState(State::DISCONNECTED);

    if (connect_.load()) {
        MINI_MUDUO_LOG_INFO(
            "Connector::retry - Retry connecting to {} in {} ms", serverAddr_.toIpPort(), retryDelay_.count());

        pOwnerLoop_->runAfter(retryDelay_, [shared_this = shared_from_this()] {
            shared_this->startInLoop();
        });

        retryDelay_ = std::min(retryDelay_ * 2, kMaxRetryDelay);
    } else {
        MINI_MUDUO_LOG_DEBUG("do not connect");
    }
}

void Connector::stop() {
    connect_.store(false);

    pOwnerLoop_->queueInLoop([this] {  // FIXME: unsafe. Why???
        this->stopInLoop();
    });
    // FIXME: cancel timer
}

void Connector::stopInLoop() {
    pOwnerLoop_->assertInLoopThread();

    if (state_ == State::CONNECTING) {
        setState(State::DISCONNECTED);

        const int sockFd = removeAndResetChannel();

        retry(sockFd);
    }
}

int Connector::removeAndResetChannel() {
    channel_->disableAll();
    channel_->remove();

    const int sockFd = channel_->fd();

    // Can't reset channel_ here, because we are inside Channel::handleEvent
    pOwnerLoop_->queueInLoop([this] {  // FIXME: unsafe. retry() can not guarantee happened after resetChannel()
        this->resetChannel();
    });

    return sockFd;
}

void Connector::resetChannel() {
    channel_.reset();
}

void Connector::restart() {
    pOwnerLoop_->assertInLoopThread();

    setState(State::DISCONNECTED);

    retryDelay_ = kInitRetryDelay;
    connect_.store(true);

    startInLoop();
}

void Connector::handleWrite() {
    MINI_MUDUO_LOG_TRACE("Connector::handleWrite state_ = {}", static_cast<int>(state_.load()));

    if (state_ == State::CONNECTING) {
        // If conected, channel_ must be reset
        // If not, retry() will re-init the channel_ with the same socktFd, which is fine
        const int sockFd = removeAndResetChannel();
        const int err = socket_ops::getSocketError(sockFd);

        if (err) {
            MINI_MUDUO_LOG_WARN("Connector::handleWrite - SO_ERROR = {} {}", err, strerror_tl(err));

            retry(sockFd);
        } else if (socket_ops::isSelfConnect(sockFd)) {
            MINI_MUDUO_LOG_WARN("Connector::handleWrite - Self connect");

            retry(sockFd);
        } else {
            setState(State::CONNECTED);

            if (connect_.load()) {
                newConnectionCallback_(sockFd);
            } else {
                socket_ops::close(sockFd);
            }
        }
    } else {
        // what happened?
        assert(state_ == State::DISCONNECTED);
    }
}

void Connector::handleError() {
    MINI_MUDUO_LOG_ERROR("Connector::handleError state = {}", static_cast<int>(state_.load()));

    if (state_ == State::CONNECTING) {
        const int sockFd = removeAndResetChannel();
        const int err = socket_ops::getSocketError(sockFd);

        MINI_MUDUO_LOG_TRACE("SO_ERROR = {} {}", err, strerror_tl(err));

        retry(sockFd);
    }
}

}  // namespace mini_muduo
