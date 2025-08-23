#ifndef MINI_MUDUO_TCP_CLIENT_H
#define MINI_MUDUO_TCP_CLIENT_H

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include <mini_muduo/inet_address.h>
#include <mini_muduo/tcp_connection.h>

namespace mini_muduo {

class Connector;

class TcpClient {
public:
    TcpClient(EventLoop *pLoop, const InetAddress &serverAddr, std::string name);
    ~TcpClient();

    TcpClient(const TcpClient &other) = delete;
    TcpClient &operator=(const TcpClient &other) = delete;

    void connect();
    void disconnect();
    void stop();

    bool retry() const {
        return retry_;
    }

    void enableRetry() {
        retry_ = true;
    }

    const std::string &name() const {
        return name_;
    }

    EventLoop *getLoop() const {
        return pOwnerLoop_;
    }

    /// Set connection callback.
    /// Not thread safe.
    void setConnectionCallback(ConnectionCallback cb) {
        connectionCb_ = std::move(cb);
    }

    /// Set message callback.
    /// Not thread safe.
    void setMessageCallback(MessageCallback cb) {
        messageCb_ = std::move(cb);
    }

    /// Set write complete callback.
    /// Not thread safe.
    void setWriteCompleteCallback(WriteCompleteCallback cb) {
        writeCompleteCb_ = std::move(cb);
    }

private:
    /// Not thread safe, but in loop
    void onNewConnection(int sockFd);

    /// Not thread safe, but in loop
    void removeConnection(const TcpConnectionPtr &conn);

    EventLoop *pOwnerLoop_;
    const std::string name_;

    int nextConnId_ = 1;
    std::atomic<bool> retry_ = false;
    std::atomic<bool> connect_ = true;

    ConnectionCallback connectionCb_;
    MessageCallback messageCb_;
    WriteCompleteCallback writeCompleteCb_;

    std::shared_ptr<Connector> connector_;

    mutable std::mutex mu_;
    TcpConnectionPtr conn_;
};

}  // namespace mini_muduo

#endif
