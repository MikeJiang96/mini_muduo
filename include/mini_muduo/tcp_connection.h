#ifndef MINI_MUDUO_TCP_CONNECTION_H
#define MINI_MUDUO_TCP_CONNECTION_H

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <mini_muduo/buffer.h>
#include <mini_muduo/callbacks.h>
#include <mini_muduo/event_loop.h>
#include <mini_muduo/inet_address.h>

namespace mini_muduo {

class Socket;

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
    friend class TcpServer;
    friend class TcpClient;

public:
    TcpConnection(
        EventLoop *pLoop, std::string name, int sockFd, const InetAddress &localAddr, const InetAddress &peerAddr);

    ~TcpConnection();

    TcpConnection(const TcpConnection &other) = delete;
    TcpConnection &operator=(const TcpConnection &other) = delete;

    EventLoop *getLoop() const {
        return pOwnerIoLoop_;
    }

    const std::string &name() const {
        return name_;
    }

    const InetAddress &localAddress() const {
        return localAddr_;
    }

    const InetAddress &peerAddress() const {
        return peerAddr_;
    }

    void setHighWaterMarkCallback(HighWaterMarkCallback cb, size_t highWaterMarkSize) {
        highWaterMarkCallback_ = std::move(cb);
        highWaterMark_ = highWaterMarkSize;
    }

    bool connected() const {
        return state_ == State::CONNECTED;
    }

    bool disconnected() const {
        return state_ == State::DISCONNECTED;
    }

    void send(const void *message, size_t len);
    void send(std::string_view message);
    void send(Buffer &buf);  // this one will swap data

    void shutdown();

    void forceClose();

    void setTcpNoDelay(bool on);

private:
    enum class State {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        DISCONNECTING
    };

    /// Internal use for TcpServer and TcpClient only.
    void setConnectionCallback(ConnectionCallback cb) {
        connectionCallback_ = std::move(cb);
    }

    void setMessageCallback(MessageCallback cb) {
        messageCallback_ = std::move(cb);
    }

    void setWriteCompleteCallback(WriteCompleteCallback cb) {
        writeCompleteCallback_ = std::move(cb);
    }

    void setCloseCallback(CloseCallback cb) {
        closeCallback_ = std::move(cb);
    }

    // called when TcpServer accepts a new connection
    void onConnectionEstablished();  // should be called only once

    // called when TcpServer has removed me from its map
    void onConnectionDestroyed();  // should be called only once

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(std::string_view message);

    void shutdownInLoop();

    void forceCloseInLoop();

    void setState(State state) {
        state_ = state;
    }

    EventLoop *pOwnerIoLoop_;
    const std::string name_;

    State state_ = State::CONNECTING;

    // Pimpl
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;

    size_t highWaterMark_ = 64 * 1024 * 1024;

    Buffer inputBuf_;
    Buffer outputBuf_;
};

}  // namespace mini_muduo

#endif
