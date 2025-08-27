#ifndef MINI_MUDUO_TCP_SERVER_H
#define MINI_MUDUO_TCP_SERVER_H

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include <mini_muduo/event_loop_thread_pool.h>
#include <mini_muduo/inet_address.h>
#include <mini_muduo/tcp_connection.h>

namespace mini_muduo {

class Acceptor;

class TcpServer {
public:
    enum class Option {
        NO_REUSE_PORT,
        REUSE_PORT,
    };

    TcpServer(EventLoop *pLoop,
              const InetAddress &listenAddr,
              std::string name,
              int nThreads = 0,
              Option option = Option::NO_REUSE_PORT);

    ~TcpServer();

    TcpServer(const TcpServer &other) = delete;
    TcpServer &operator=(const TcpServer &other) = delete;

    /// Starts the server if it's not listening.
    ///
    /// It's harmless to call it multiple times.
    /// Thread safe.
    void start();

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
    using ConnectionHashMap = std::unordered_map<std::string, TcpConnectionPtr>;

    /// Not thread safe, but in loop
    void onNewConnection(int sockFd, const InetAddress &peerAddr);

    /// Thread safe.
    void removeConnection(const TcpConnectionPtr &conn);

    /// Not thread safe, but in loop
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    EventLoop *pOwnerMainLoop_;
    const std::string name_;
    const std::string ipPort_;

    EventLoopThreadPool threadPool_;

    std::once_flag startOnce_;

    // Pimpl
    const std::unique_ptr<Acceptor> acceptor_;

    ConnectionCallback connectionCb_ = defaultConnectionCallback;
    MessageCallback messageCb_ = defaultMessageCallback;
    WriteCompleteCallback writeCompleteCb_;

    int nextConnId_ = 1;
    ConnectionHashMap connections_;
};

}  // namespace mini_muduo

#endif
