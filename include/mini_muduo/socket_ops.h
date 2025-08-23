#ifndef MINI_MUDUO_SOCKET_OPS_H
#define MINI_MUDUO_SOCKET_OPS_H

#include <arpa/inet.h>
#include <sys/uio.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <mini_muduo/endian.h>
#include <mini_muduo/log.h>

namespace mini_muduo {
namespace socket_ops {

inline int createNonblockingOrDie(sa_family_t family) {
    int sockFd = ::socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);

    if (sockFd < 0) {
        MINI_MUDUO_LOG_CRITITAL("socket()");
        ::exit(EXIT_FAILURE);
    }

    return sockFd;
}

inline void bindOrDie(int sockFd, const struct sockaddr *addr) {
    int ret = ::bind(sockFd, addr, static_cast<socklen_t>(sizeof(struct sockaddr_in6)));

    if (ret < 0) {
        MINI_MUDUO_LOG_CRITITAL("bind()");
        ::exit(EXIT_FAILURE);
    }
}

inline void listenOrDie(int sockFd) {
    int ret = ::listen(sockFd, SOMAXCONN);

    if (ret < 0) {
        MINI_MUDUO_LOG_CRITITAL("listen()");
        ::exit(EXIT_FAILURE);
    }
}

inline int accept(int sockFd, struct sockaddr_in6 *addr) {
    auto addrlen = static_cast<socklen_t>(sizeof(*addr));
    int connFd = ::accept4(sockFd, reinterpret_cast<struct sockaddr *>(addr), &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);

    if (connFd < 0) {
        int savedErrno = errno;

        switch (savedErrno) {
            case EAGAIN:
            case ECONNABORTED:
            case EINTR:
            case EPROTO:  // ???
            case EPERM:
            case EMFILE:  // per-process lmit of open file desctiptor ???
                // expected errors
                errno = savedErrno;
                break;
            case EBADF:
            case EFAULT:
            case EINVAL:
            case ENFILE:
            case ENOBUFS:
            case ENOMEM:
            case ENOTSOCK:
            case EOPNOTSUPP:
                // unexpected errors
                break;
            default:
                break;
        }
    }

    return connFd;
}

inline int connect(int sockFd, const struct sockaddr *addr) {
    return ::connect(sockFd, addr, static_cast<socklen_t>(sizeof(struct sockaddr_in6)));
}

inline ssize_t read(int sockFd, void *buf, size_t count) {
    return ::read(sockFd, buf, count);
}

inline ssize_t readv(int sockFd, const struct iovec *iov, int iovcnt) {
    return ::readv(sockFd, iov, iovcnt);
}

inline ssize_t write(int sockFd, const void *buf, size_t count) {
    return ::write(sockFd, buf, count);
}

inline void close(int sockFd) {
    if (::close(sockFd) < 0) {
        MINI_MUDUO_LOG_ERROR("close()");
    }
}

inline void shutdownWrite(int sockFd) {
    if (::shutdown(sockFd, SHUT_WR) < 0) {
        MINI_MUDUO_LOG_ERROR("shutdown()");
    }
}

inline void toIp(char *buf, size_t size, const struct sockaddr *addr) {
    if (addr->sa_family == AF_INET) {
        assert(size >= INET_ADDRSTRLEN);

        auto addr4 = reinterpret_cast<const struct sockaddr_in *>(addr);
        ::inet_ntop(AF_INET, &addr4->sin_addr, buf, static_cast<socklen_t>(size));
    } else if (addr->sa_family == AF_INET6) {
        assert(size >= INET6_ADDRSTRLEN);

        auto addr6 = reinterpret_cast<const struct sockaddr_in6 *>(addr);
        ::inet_ntop(AF_INET6, &addr6->sin6_addr, buf, static_cast<socklen_t>(size));
    } else {
        MINI_MUDUO_LOG_ERROR("Invalid sa_family = {}", addr->sa_family);
    }
}

inline void toIpPort(char *buf, size_t size, const struct sockaddr *addr) {
    if (addr->sa_family == AF_INET6) {
        buf[0] = '[';
        toIp(buf + 1, size - 1, addr);

        size_t end = ::strlen(buf);
        auto addr6 = reinterpret_cast<const struct sockaddr_in6 *>(addr);
        uint16_t port = socket_ops::hostToNetwork16(addr6->sin6_port);

        assert(size > end);

        snprintf(buf + end, size - end, "]:%u", port);

        return;
    }

    toIp(buf, size, addr);

    size_t end = ::strlen(buf);
    auto addr4 = reinterpret_cast<const struct sockaddr_in *>(addr);
    uint16_t port = socket_ops::hostToNetwork16(addr4->sin_port);

    assert(size > end);

    snprintf(buf + end, size - end, ":%u", port);
}

inline void fromIpPort(const char *ip, uint16_t port, struct sockaddr_in *addr) {
    addr->sin_family = AF_INET;
    addr->sin_port = socket_ops::hostToNetwork16(port);

    if (::inet_pton(AF_INET, ip, &addr->sin_addr) <= 0) {
        MINI_MUDUO_LOG_ERROR("inet_pton()");
    }
}

inline void fromIpPort(const char *ip, uint16_t port, struct sockaddr_in6 *addr) {
    addr->sin6_family = AF_INET6;
    addr->sin6_port = socket_ops::hostToNetwork16(port);

    if (::inet_pton(AF_INET6, ip, &addr->sin6_addr) <= 0) {
        MINI_MUDUO_LOG_ERROR("inet_pton()");
    }
}

inline int getSocketError(int sockFd) {
    int optval = 0;
    auto optlen = static_cast<socklen_t>(sizeof(optval));

    if (::getsockopt(sockFd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
        return errno;
    } else {
        return optval;
    }
}

inline struct sockaddr_in6 getLocalAddr(int sockFd) {
    struct sockaddr_in6 localAddr;

    memset(&localAddr, 0, sizeof(localAddr));

    auto addrlen = static_cast<socklen_t>(sizeof(localAddr));

    if (::getsockname(sockFd, reinterpret_cast<struct sockaddr *>(&localAddr), &addrlen) < 0) {
        MINI_MUDUO_LOG_ERROR("getsockname()");
    }

    return localAddr;
}

inline struct sockaddr_in6 getPeerAddr(int sockFd) {
    struct sockaddr_in6 peerAddr;

    memset(&peerAddr, 0, sizeof(peerAddr));

    auto addrlen = static_cast<socklen_t>(sizeof(peerAddr));

    if (::getpeername(sockFd, reinterpret_cast<struct sockaddr *>(&peerAddr), &addrlen) < 0) {
        MINI_MUDUO_LOG_ERROR("getpeername()");
    }

    return peerAddr;
}

inline bool isSelfConnect(int sockFd) {
    struct sockaddr_in6 localaddr = getLocalAddr(sockFd);
    struct sockaddr_in6 peeraddr = getPeerAddr(sockFd);

    if (localaddr.sin6_family == AF_INET) {
        const struct sockaddr_in *laddr4 = reinterpret_cast<struct sockaddr_in *>(&localaddr);
        const struct sockaddr_in *raddr4 = reinterpret_cast<struct sockaddr_in *>(&peeraddr);
        return laddr4->sin_port == raddr4->sin_port && laddr4->sin_addr.s_addr == raddr4->sin_addr.s_addr;
    } else if (localaddr.sin6_family == AF_INET6) {
        return localaddr.sin6_port == peeraddr.sin6_port &&
               memcmp(&localaddr.sin6_addr, &peeraddr.sin6_addr, sizeof localaddr.sin6_addr) == 0;
    } else {
        return false;
    }
}

}  // namespace socket_ops
}  // namespace mini_muduo

#endif
