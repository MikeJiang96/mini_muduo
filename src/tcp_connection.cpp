#include <mini_muduo/tcp_connection.h>

#include <cassert>
#include <cstddef>
#include <cstring>
#include <memory>
#include <utility>

#include "socket.h"

#include <mini_muduo/channel.h>
#include <mini_muduo/event_loop.h>
#include <mini_muduo/log.h>
#include <mini_muduo/socket_ops.h>
#include <mini_muduo/timestamp.h>

namespace mini_muduo {

void defaultConnectionCallback(const TcpConnectionPtr &conn) {
    (void)conn;

    MINI_MUDUO_LOG_TRACE("{} - > {} is {}",
                         conn->localAddress().toIpPort(),
                         conn->peerAddress().toIpPort(),
                         conn->connected() ? "UP" : "DOWN");

    // do not call conn->forceClose(), because some users want to register message callback only.
}

void defaultMessageCallback(const TcpConnectionPtr &, Buffer &buf, Timestamp) {
    buf.retrieveAll();
}

TcpConnection::TcpConnection(
    EventLoop *pLoop, std::string name, int sockFd, const InetAddress &localAddr, const InetAddress &peerAddr)
    : pOwnerIoLoop_(pLoop)
    , name_(std::move(name))
    , socket_(std::make_unique<Socket>(sockFd))
    , channel_(std::make_unique<Channel>(pLoop, sockFd))
    , localAddr_(localAddr)
    , peerAddr_(peerAddr) {
    channel_->setReadCallback([this](Timestamp receiveTime) {
        this->handleRead(receiveTime);
    });

    channel_->setWriteCallback([this] {
        this->handleWrite();
    });

    channel_->setCloseCallback([this] {
        this->handleClose();
    });

    channel_->setErrorCallback([this] {
        this->handleError();
    });

    MINI_MUDUO_LOG_DEBUG("TcpConnection::ctor[{}], fd = {}", name_, sockFd);

    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection() {
    MINI_MUDUO_LOG_DEBUG(
        "TcpConnection::dtor[{}], fd = {}, state = {}", name_, socket_->fd(), static_cast<int>(state_));

    assert(state_ == State::DISCONNECTED);
}

void TcpConnection::onConnectionEstablished() {
    pOwnerIoLoop_->assertInLoopThread();

    assert(state_ == State::CONNECTING);

    assert(connectionCallback_);
    assert(messageCallback_);
    assert(closeCallback_);

    setState(State::CONNECTED);

    // No need to tie()???
    // channel_->tie(shared_from_this());
    channel_->enableReading();

    connectionCallback_(shared_from_this());
}

void TcpConnection::onConnectionDestroyed() {
    pOwnerIoLoop_->assertInLoopThread();

    // Maybe be force closed before
    if (state_ == State::CONNECTED) {
        setState(State::DISCONNECTED);

        channel_->disableAll();

        connectionCallback_(shared_from_this());
    }

    channel_->remove();
}

void TcpConnection::send(const void *data, size_t len) {
    send(std::string_view(static_cast<const char *>(data), len));
}

void TcpConnection::send(std::string_view message) {
    if (state_ != State::CONNECTED) {
        return;
    }

    if (pOwnerIoLoop_->isInLoopThread()) {
        sendInLoop(message);
    } else {
        pOwnerIoLoop_->runInLoop([shared_this = shared_from_this(), message = std::string(message)] {
            shared_this->sendInLoop(message);
        });
    }
}

// FIXME efficiency!!!
void TcpConnection::send(Buffer &buf) {
    if (state_ != State::CONNECTED) {
        return;
    }

    if (pOwnerIoLoop_->isInLoopThread()) {
        sendInLoop(buf.toStringView());
        buf.retrieveAll();
    } else {
        pOwnerIoLoop_->runInLoop([shared_this = shared_from_this(), message = buf.retrieveAllAsString()] {
            shared_this->sendInLoop(message);
        });
    }
}

void TcpConnection::sendInLoop(std::string_view message) {
    pOwnerIoLoop_->assertInLoopThread();

    if (state_ == State::DISCONNECTED) {
        MINI_MUDUO_LOG_WARN("Already Disconnnected");
        return;
    }

    const void *data = message.data();
    const size_t len = message.size();

    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    // if nothing in output queue, try writing directly
    if (!channel_->isWriting() && outputBuf_.readableBytes() == 0) {
        nwrote = socket_ops::write(channel_->fd(), data, len);

        if (nwrote >= 0) {
            remaining = len - static_cast<size_t>(nwrote);

            if (remaining == 0 && writeCompleteCallback_) {
                pOwnerIoLoop_->queueInLoop([shared_this = shared_from_this()] {
                    shared_this->writeCompleteCallback_(shared_this);
                });
            }
        } else {  // nwrote < 0
            nwrote = 0;

            if (errno != EWOULDBLOCK) {
                if (errno == EPIPE || errno == ECONNRESET) {  // FIXME: any others?
                    faultError = true;
                }
            }
        }
    }

    assert(remaining <= len);

    if (!faultError && remaining > 0) {
        const size_t oldLen = outputBuf_.readableBytes();

        outputBuf_.append(static_cast<const char *>(data) + nwrote, remaining);

        if (outputBuf_.readableBytes() >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_) {
            pOwnerIoLoop_->queueInLoop([shared_this = shared_from_this(), outBufSize = oldLen + remaining] {
                shared_this->highWaterMarkCallback_(shared_this, outBufSize);
            });
        }

        if (!channel_->isWriting()) {
            channel_->enableWriting();
        }
    }
}

void TcpConnection::handleRead(Timestamp receiveTime) {
    pOwnerIoLoop_->assertInLoopThread();

    // Why savedErrno?
    int savedErrno = 0;
    const ssize_t n = inputBuf_.readFd(channel_->fd(), &savedErrno);

    if (n > 0) {
        messageCallback_(shared_from_this(), inputBuf_, receiveTime);
    } else if (n == 0) {
        handleClose();
    } else {
        errno = savedErrno;

        handleError();
    }
}

void TcpConnection::handleWrite() {
    pOwnerIoLoop_->assertInLoopThread();

    if (channel_->isWriting()) {
        const ssize_t n = socket_ops::write(channel_->fd(), outputBuf_.peek(), outputBuf_.readableBytes());

        if (n > 0) {
            outputBuf_.retrieve(static_cast<size_t>(n));

            if (outputBuf_.readableBytes() == 0) {
                channel_->disableWriting();

                if (writeCompleteCallback_) {
                    pOwnerIoLoop_->queueInLoop([shared_this = shared_from_this()] {
                        shared_this->writeCompleteCallback_(shared_this);
                    });
                }

                if (state_ == State::DISCONNECTING) {
                    shutdownInLoop();
                }
            }
        } else {
            MINI_MUDUO_LOG_ERROR("write()");
        }
    } else {
        MINI_MUDUO_LOG_TRACE("Connection fd {} is down, no more writing", channel_->fd());
    }
}

void TcpConnection::shutdown() {
    if (state_ != State::CONNECTED) {
        return;
    }

    setState(State::DISCONNECTING);

    pOwnerIoLoop_->runInLoop([shared_this = shared_from_this()] {
        shared_this->shutdownInLoop();
    });
}

void TcpConnection::shutdownInLoop() {
    pOwnerIoLoop_->assertInLoopThread();

    if (!channel_->isWriting()) {
        // we are not writing
        socket_->shutdownWrite();
    }
}

void TcpConnection::forceClose() {
    if (state_ == State::CONNECTED || state_ == State::DISCONNECTING) {
        setState(State::DISCONNECTING);

        pOwnerIoLoop_->queueInLoop(std::bind(&TcpConnection::forceCloseInLoop, shared_from_this()));
    }
}

void TcpConnection::forceCloseInLoop() {
    pOwnerIoLoop_->assertInLoopThread();

    if (state_ == State::CONNECTED || state_ == State::DISCONNECTING) {
        // as if we received 0 byte in handleRead();
        handleClose();
    }
}

void TcpConnection::setTcpNoDelay(bool on) {
    socket_->setTcpNoDelay(on);
}

void TcpConnection::handleClose() {
    pOwnerIoLoop_->assertInLoopThread();

    MINI_MUDUO_LOG_TRACE("fd = {}, state = {}", channel_->fd(), static_cast<int>(state_));

    assert(state_ == State::CONNECTED || state_ == State::DISCONNECTING);

    // we don't close fd, leave it to dtor, so we can find leaks easily.
    setState(State::DISCONNECTED);

    channel_->disableAll();

    TcpConnectionPtr guardThis(shared_from_this());

    connectionCallback_(guardThis);

    // must be the last line
    // Call TcpServer::removeConnection() or TcpClient::removeConnection() with guardThis
    closeCallback_(guardThis);
}

void TcpConnection::handleError() {
    const int err = socket_ops::getSocketError(channel_->fd());

    MINI_MUDUO_LOG_ERROR("Socket {} {}", name_, strerror_tl(err));
}

}  // namespace mini_muduo
