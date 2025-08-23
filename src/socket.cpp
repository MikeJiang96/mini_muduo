#include "socket.h"

#include <netinet/tcp.h>

#include <cstring>

#include <mini_muduo/log.h>
#include <mini_muduo/socket_ops.h>

namespace mini_muduo {

Socket::~Socket() {
    socket_ops::close(sockFd_);
}

void Socket::bind(const InetAddress &addr) const {
    socket_ops::bindOrDie(sockFd_, addr.getSockAddr());
}

void Socket::listen() const {
    socket_ops::listenOrDie(sockFd_);
}

int Socket::accept(InetAddress *peeraddr) const {
    struct sockaddr_in6 addr;

    memset(&addr, 0, sizeof(addr));

    const int connFd = socket_ops::accept(sockFd_, &addr);

    if (connFd >= 0) {
        peeraddr->setSockAddrInet6(addr);
    }

    return connFd;
}

void Socket::shutdownWrite() const {
    socket_ops::shutdownWrite(sockFd_);
}

void Socket::setTcpNoDelay(bool on) const {
    int optval = on ? 1 : 0;
    ::setsockopt(sockFd_, IPPROTO_TCP, TCP_NODELAY, &optval, static_cast<socklen_t>(sizeof optval));
}

void Socket::setReuseAddr(bool on) const {
    int optval = on ? 1 : 0;
    ::setsockopt(sockFd_, SOL_SOCKET, SO_REUSEADDR, &optval, static_cast<socklen_t>(sizeof optval));
}

void Socket::setReusePort(bool on) const {
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(sockFd_, SOL_SOCKET, SO_REUSEPORT, &optval, static_cast<socklen_t>(sizeof optval));

    if (ret < 0 && on) {
        MINI_MUDUO_LOG_ERROR("SO_REUSEPORT fd = {}", sockFd_);
    }
}

void Socket::setKeepAlive(bool on) const {
    int optval = on ? 1 : 0;
    ::setsockopt(sockFd_, SOL_SOCKET, SO_KEEPALIVE, &optval, static_cast<socklen_t>(sizeof optval));
}

}  // namespace mini_muduo
