#ifndef MINI_MUDUO_EXAMPLES_HIREDIS_DEMO_HIREDIS_WRAPPER_H
#define MINI_MUDUO_EXAMPLES_HIREDIS_DEMO_HIREDIS_WRAPPER_H

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <hiredis/async.h>
#include <hiredis/hiredis.h>

#include <mini_muduo/event_loop.h>
#include <mini_muduo/inet_address.h>
#include <mini_muduo/timestamp.h>

namespace mini_muduo {

void printRedisReply(const redisReply *reply);

// Not thread-safe
class HiredisWrapper {
public:
    using ConnectCallback = std::function<void()>;
    using DisconnectCallback = std::function<void(int)>;
    using CommandCallback = std::function<void(redisReply *)>;
    using AsyncPushCallback = std::function<void(redisReply *)>;

    using FV = std::pair<std::string, std::vector<char>>;
    using RedisHash = std::pair<std::string, std::vector<FV>>;

    HiredisWrapper(EventLoop *pLoop, const InetAddress &serverAddr);
    ~HiredisWrapper();

    HiredisWrapper(const HiredisWrapper &other) = delete;
    HiredisWrapper &operator=(const HiredisWrapper &other) = delete;

    int connect();
    void disconnect();

    bool connected() const;

    EventLoop *getLoop() const {
        return pOwerLoop_;
    }

    bool checkReply() const {
        return checkReply_;
    }

    void setConnectCallback(ConnectCallback cb) {
        connectCb_ = std::move(cb);
    }

    void setDisconnectCallback(DisconnectCallback cb) {
        disconnectCb_ = std::move(cb);
    }

    void setAsyncPushCallBack(AsyncPushCallback cb);

    int command(CommandCallback cb, const char *cmd, ...);

    // Caller must ensure cb is valid
    int commandWithPointerToCallback(CommandCallback *pCb, const char *cmd, ...);

    int ping();

    // TODO: Support full TTL option
    int setKey(const std::string &key,
               const std::vector<char> value,
               CommandCallback cb,
               std::chrono::seconds ttl = std::chrono::seconds(0));

    int getKey(const std::string &key, CommandCallback cb);

    int subscribe(const std::string &channelName, CommandCallback cb);

private:
    using SubscribeCommandCallbackHashMap = std::unordered_map<std::string, CommandCallback>;

    void handleRead(Timestamp receiveTime);
    void handleWrite();

    void logConnection(bool up) const;
    int fd() const;

    void setChannel();
    void removeAndResetChannel();
    void resetChannel();

    void onConnect(int status);
    void onDisconnect(int status);
    void onAsyncPushCallback(redisReply *pReply);

    static HiredisWrapper *getHiredisWrapper(const redisAsyncContext *ac);

    // To reg, will call the non-static version
    static void connectCallback(const redisAsyncContext *ac, int status);
    static void disconnectCallback(const redisAsyncContext *ac, int status);
    static void commandCallback(redisAsyncContext *ac, void *reply, void *privdata);
    static void commandWithPointerToCallback(redisAsyncContext *ac, void *reply, void *privdata);
    static void asyncPushCallback(redisAsyncContext *ac, void *reply);

    static void addRead(void *privdata);
    static void delRead(void *privdata);
    static void addWrite(void *privdata);
    static void delWrite(void *privdata);
    static void cleanup(void *privdata);

    void pingCallback(redisReply *reply);

    EventLoop *pOwerLoop_;
    const InetAddress serverAddr_;

    redisAsyncContext *context_ = nullptr;

    std::unique_ptr<Channel> channel_;

    bool checkReply_ = true;

    ConnectCallback connectCb_;
    DisconnectCallback disconnectCb_;
    AsyncPushCallback asyncPushCb_;

    SubscribeCommandCallbackHashMap subCmdCbs_;
};

}  // namespace mini_muduo

#endif
