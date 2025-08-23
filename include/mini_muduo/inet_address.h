#ifndef MINI_MUDUO_INET_ADDRESS_H
#define MINI_MUDUO_INET_ADDRESS_H

#include <netinet/in.h>

#include <cstdint>
#include <string>

namespace mini_muduo {

class InetAddress {
public:
    /// Constructs an endpoint with given port number.
    /// Mostly used in TcpServer listening.
    explicit InetAddress(uint16_t port = 0, bool loopbackOnly = false, bool ipv6 = false);

    /// Constructs an endpoint with given ip and port.
    /// @c ip should be "1.2.3.4"
    InetAddress(std::string ip, uint16_t port, bool ipv6 = false);

    /// Constructs an endpoint with given struct @c sockaddr_in
    /// Mostly used when accepting new connections
    explicit InetAddress(const struct sockaddr_in &addr)
        : addr_(addr) {}

    explicit InetAddress(const struct sockaddr_in6 &addr)
        : addr6_(addr) {}

    InetAddress(const InetAddress &other) = default;
    InetAddress &operator=(const InetAddress &other) = default;

    ~InetAddress() = default;

    sa_family_t family() const {
        return addr_.sin_family;
    }

    std::string toIp() const;

    std::string toIpPort() const;

    uint16_t port() const;

    uint16_t portNetEndian() const {
        return addr_.sin_port;
    }

    const struct sockaddr *getSockAddr() const {
        return reinterpret_cast<const struct sockaddr *>(&addr6_);
    }

    void setSockAddrInet6(const struct sockaddr_in6 &addr6) {
        addr6_ = addr6;
    }

    static bool resolve(const std::string &hostname, InetAddress &out);

private:
    union {
        struct sockaddr_in addr_;
        struct sockaddr_in6 addr6_;
    };
};

}  // namespace mini_muduo

#endif
