#ifndef MINI_MUDUO_EXAMPLES_ECHO_ECHO_H
#define MINI_MUDUO_EXAMPLES_ECHO_ECHO_H

#include <mini_muduo/event_loop.h>
#include <mini_muduo/tcp_server.h>

// RFC 862
class EchoServer {
public:
    EchoServer(mini_muduo::EventLoop *loop, const mini_muduo::InetAddress &listenAddr);

    void start();  // calls server_.start();

private:
    void onConnection(const mini_muduo::TcpConnectionPtr &conn);

    void onMessage(const mini_muduo::TcpConnectionPtr &conn,
                   mini_muduo::Buffer &buf,
                   mini_muduo::Timestamp receiveTime);

    mini_muduo::TcpServer server_;
};

#endif  // MINI_MUDUO_EXAMPLES_ECHO_H
