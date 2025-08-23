#include "echo/echo.h"

#include <mini_muduo/log.h>

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

EchoServer::EchoServer(mini_muduo::EventLoop *loop, const mini_muduo::InetAddress &listenAddr)
    : server_(loop, listenAddr, "EchoServer", 2) {
    server_.setConnectionCallback(std::bind(&EchoServer::onConnection, this, _1));
    server_.setMessageCallback(std::bind(&EchoServer::onMessage, this, _1, _2, _3));
}

void EchoServer::start() {
    server_.start();
}

void EchoServer::onConnection(const mini_muduo::TcpConnectionPtr &conn) {
    MINI_MUDUO_LOG_INFO("EchoServer - {} - > {} is {}",
                        conn->peerAddress().toIpPort(),
                        conn->localAddress().toIpPort(),
                        conn->connected() ? "UP" : "DOWN");
}

void EchoServer::onMessage(const mini_muduo::TcpConnectionPtr &conn,
                           mini_muduo::Buffer &buf,
                           mini_muduo::Timestamp receiveTime) {
    std::string msg(buf.retrieveAllAsString());

    MINI_MUDUO_LOG_INFO("{} echo {} bytes, data received at {}", conn->name(), msg.size(), receiveTime.toString());

    conn->send(msg);
}
