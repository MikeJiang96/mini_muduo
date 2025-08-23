#ifndef MINI_MUDUO_ACCEPTOR_H
#define MINI_MUDUO_ACCEPTOR_H

#include <functional>
#include <utility>

#include "socket.h"

#include <mini_muduo/channel.h>
#include <mini_muduo/event_loop.h>
#include <mini_muduo/inet_address.h>

namespace mini_muduo {

class Acceptor {
public:
    using NewConnectionCallback = std::function<void(int sockFd, const InetAddress &)>;

    Acceptor(EventLoop *pLoop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

    Acceptor(const Acceptor &other) = delete;
    Acceptor &operator=(const Acceptor &other) = delete;

    void setNewConnectionCallback(NewConnectionCallback cb) {
        newConnectionCallback_ = std::move(cb);
    }

    void listen();

    bool listening() const {
        return listening_;
    }

private:
    void handleRead();

    EventLoop *pOwnerLoop_;

    Socket acceptSocket_;
    Channel acceptChannel_;

    bool listening_ = false;

    NewConnectionCallback newConnectionCallback_;

    int idleFd_;
};

}  // namespace mini_muduo

#endif
