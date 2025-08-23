#include <mini_muduo/inet_address.h>

#include <netdb.h>

#include <cstring>

#include <mini_muduo/endian.h>
#include <mini_muduo/log.h>
#include <mini_muduo/socket_ops.h>

namespace mini_muduo {

static_assert(sizeof(InetAddress) == sizeof(struct sockaddr_in6), "InetAddress is same size as sockaddr_in6");
static_assert(offsetof(sockaddr_in, sin_family) == 0, "sin_family offset 0");
static_assert(offsetof(sockaddr_in6, sin6_family) == 0, "sin6_family offset 0");
static_assert(offsetof(sockaddr_in, sin_port) == 2, "sin_port offset 2");
static_assert(offsetof(sockaddr_in6, sin6_port) == 2, "sin6_port offset 2");

InetAddress::InetAddress(uint16_t portArg, bool loopbackOnly, bool ipv6) {
    static_assert(offsetof(InetAddress, addr6_) == 0, "addr6_ offset 0");
    static_assert(offsetof(InetAddress, addr_) == 0, "addr_ offset 0");

    if (ipv6) {
        memset(&addr6_, 0, sizeof(addr6_));
        addr6_.sin6_family = AF_INET6;
        in6_addr ip = loopbackOnly ? in6addr_loopback : in6addr_any;
        addr6_.sin6_addr = ip;
        addr6_.sin6_port = socket_ops::hostToNetwork16(portArg);
    } else {
        memset(&addr_, 0, sizeof(addr_));
        addr_.sin_family = AF_INET;
        in_addr_t ip = loopbackOnly ? INADDR_LOOPBACK : INADDR_ANY;
        addr_.sin_addr.s_addr = socket_ops::hostToNetwork32(ip);
        addr_.sin_port = socket_ops::hostToNetwork16(portArg);
    }
}

InetAddress::InetAddress(std::string ip, uint16_t portArg, bool ipv6) {
    if (ipv6 || strchr(ip.c_str(), ':')) {
        memset(&addr6_, 0, sizeof(addr6_));
        socket_ops::fromIpPort(ip.c_str(), portArg, &addr6_);
    } else {
        memset(&addr_, 0, sizeof(addr_));
        socket_ops::fromIpPort(ip.c_str(), portArg, &addr_);
    }
}

std::string InetAddress::toIp() const {
    char buf[64] = "";
    socket_ops::toIp(buf, sizeof buf, getSockAddr());
    return buf;
}

std::string InetAddress::toIpPort() const {
    char buf[64] = "";
    socket_ops::toIpPort(buf, sizeof buf, getSockAddr());
    return buf;
}

uint16_t InetAddress::port() const {
    return socket_ops::networkToHost16(portNetEndian());
}

/* static */ bool InetAddress::resolve(const std::string &hostname, InetAddress &out) {
    static thread_local char t_resolveBuffer[64 * 1024];

    struct hostent hent;
    struct hostent *he = nullptr;
    int herrno = 0;

    memset(&hent, 0, sizeof(hent));

    int ret = gethostbyname_r(hostname.c_str(), &hent, t_resolveBuffer, sizeof t_resolveBuffer, &he, &herrno);
    if (ret == 0 && he != nullptr) {
        assert(he->h_addrtype == AF_INET && he->h_length == sizeof(uint32_t));

        out.addr_.sin_addr = *reinterpret_cast<struct in_addr *>(he->h_addr);
        return true;
    } else {
        if (ret) {
            MINI_MUDUO_LOG_ERROR("InetAddress::resolve");
        }

        return false;
    }
}

}  // namespace mini_muduo
