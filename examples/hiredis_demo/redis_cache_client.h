#ifndef MINI_MUDUO_EXAMPLES_HIREDIS_DEMO_REDIS_CACHE_CLIENT_H
#define MINI_MUDUO_EXAMPLES_HIREDIS_DEMO_REDIS_CACHE_CLIENT_H

#include <condition_variable>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <utility>

#include <hiredis/hiredis.h>

#include <mini_muduo/inet_address.h>

#include <hiredis_demo/hiredis_wrapper.h>

namespace mini_muduo {

// RESP3 protocol, default mode
// Using one connection for getting keys and receiving invalidated PUSH messages, the other for setting keys
// So users should take care of the set-"PUSH invalidated" loop
class RedisCacheClient {
public:
    using GetKeyCallback = std::function<void(const std::string &key, const std::vector<char> &value)>;
    using KeyInvalidatedCallback = std::function<void(const std::string &key)>;
    using SetCompleteCallback = std::function<void(const std::string &key)>;
    using DisconnectCallback = std::function<void(int)>;

    RedisCacheClient(EventLoop *pLoop, const InetAddress &serverAddr)
        : cacheConn_(pLoop, serverAddr)
        , writeConn_(pLoop, serverAddr) {}

    ~RedisCacheClient() = default;

    RedisCacheClient(const RedisCacheClient &other) = delete;
    RedisCacheClient &operator=(const RedisCacheClient &other) = delete;

    void setDisconnectCallback(DisconnectCallback cb) {
        disconnectCb_ = std::move(cb);
    }

    // Call only once
    int start();

    void disconnect() {
        cacheConn_.disconnect();
        writeConn_.disconnect();
    }

    // Not thread-safe
    void trackKey(const std::string &key, GetKeyCallback getKeyCb, KeyInvalidatedCallback keyInvalidatedCb);

    int setKey(const std::string &key,
               const std::vector<char> value,
               SetCompleteCallback cb = {},
               std::chrono::seconds ttl = std::chrono::seconds(0));

private:
    // Call GetKeyCallback in HiredisWrapper::CommandCallback
    using GetKeyCallbackHashMap = std::unordered_map<std::string, HiredisWrapper::CommandCallback>;
    using KeyInvalidatedCallbackHashMap = std::unordered_map<std::string, KeyInvalidatedCallback>;

    using RegTrackTimerHashMap = std::unordered_map<std::string, TimerId>;

    void onHello3(redisReply *reply);
    void onPush(redisReply *reply);

    HiredisWrapper cacheConn_;
    HiredisWrapper writeConn_;

    DisconnectCallback disconnectCb_;

    GetKeyCallbackHashMap getKeyCbs_;
    KeyInvalidatedCallbackHashMap keyInvalidatedCbs_;

    RegTrackTimerHashMap regTrackTimers_;

    std::once_flag startOnce_;

    std::mutex mu_;
    std::condition_variable cond_;
    bool inited_ = false;
};

}  // namespace mini_muduo

#endif
