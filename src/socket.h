#ifndef MINI_MUDUO_SOCKET_H
#define MINI_MUDUO_SOCKET_H

#include <mini_muduo/inet_address.h>

namespace mini_muduo {

class Socket {
public:
    explicit Socket(int sockFd)
        : sockFd_(sockFd) {}

    ~Socket();

    Socket(const Socket &other) = delete;
    Socket &operator=(const Socket &other) = delete;

    int fd() const {
        return sockFd_;
    }

    /// abort if address in use
    void bind(const InetAddress &localaddr) const;

    /// abort if address in use
    void listen() const;

    /// On success, returns a non-negative integer that is
    /// a descriptor for the accepted socket, which has been
    /// set to non-blocking and close-on-exec. *peeraddr is assigned.
    /// On error, -1 is returned, and *peeraddr is untouched.
    int accept(InetAddress *peeraddr) const;

    void shutdownWrite() const;

    ///
    /// Enable/disable TCP_NODELAY (disable/enable Nagle's algorithm).
    ///
    void setTcpNoDelay(bool on) const;

    ///
    /// Enable/disable SO_REUSEADDR
    ///
    void setReuseAddr(bool on) const;

    ///
    /// Enable/disable SO_REUSEPORT
    ///
    void setReusePort(bool on) const;

    ///
    /// Enable/disable SO_KEEPALIVE
    ///
    void setKeepAlive(bool on) const;

private:
    const int sockFd_;
};

}  // namespace mini_muduo

#endif
