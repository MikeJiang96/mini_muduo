#include <cstddef>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <vector>

#include <hiredis/read.h>

#include <mini_muduo/channel.h>
#include <mini_muduo/event_loop.h>
#include <mini_muduo/inet_address.h>
#include <mini_muduo/log.h>

#include <hiredis_demo/hiredis_wrapper.h>
#include <hiredis_demo/redis_cache_client.h>

using namespace mini_muduo;

[[maybe_unused]]
static std::vector<std::shared_ptr<HiredisWrapper>> testHiredisWrappers(EventLoop *pLoop,
                                                                        const InetAddress &serverAddr) {
    std::vector<std::shared_ptr<HiredisWrapper>> ret;

    const auto ping = std::make_shared<HiredisWrapper>(pLoop, serverAddr);

    ping->setConnectCallback([pPing = ping.get()] {
        (void)pPing->ping();

        pPing->disconnect();
    });

    const auto getThenPing = std::make_shared<HiredisWrapper>(pLoop, serverAddr);

    getThenPing->setConnectCallback([pGetThenPing = getThenPing.get()] {
        (void)pGetThenPing->getKey("k1", [pGetThenPing](redisReply *reply) {
            if (reply->type == REDIS_REPLY_NIL) {
                MINI_MUDUO_LOG_WARN("GET k1 failed");
            } else if (reply->type == REDIS_REPLY_STRING) {
                MINI_MUDUO_LOG_INFO("k1 = {}", reply->str);
            }

            (void)pGetThenPing->ping();
            pGetThenPing->disconnect();
        });
    });

    const auto sub = std::make_shared<HiredisWrapper>(pLoop, serverAddr);

    sub->setConnectCallback([pSub = sub.get()] {
        const std::string channel1 = "test1";
        const std::string channel2 = "test2";

        (void)pSub->subscribe(channel1, [pSub](redisReply *reply) {
            static int count = 0;

            if (count++ > 2) {
                pSub->disconnect();
                return;
            }

            const auto element = reply->element;

            if (::strcmp(element[0]->str, "message") == 0) {
                for (size_t i = 0; i < reply->elements; i++) {
                    MINI_MUDUO_LOG_INFO("hahaha: reply->element[{}] = {}", i, element[i]->str);
                }
            }
        });

        (void)pSub->subscribe(channel2, [](redisReply *reply) {
            const auto element = reply->element;

            if (::strcmp(element[0]->str, "message") == 0) {
                for (size_t i = 0; i < reply->elements; i++) {
                    MINI_MUDUO_LOG_INFO("xixixi: reply->element[{}] = {}", i, element[i]->str);
                }
            }
        });
    });

    sub->setDisconnectCallback([pLoop](int status) {
        (void)status;

        pLoop->quit();
    });

    ret.push_back(std::move(ping));
    ret.push_back(std::move(getThenPing));
    ret.push_back(std::move(sub));

    return ret;
}

[[maybe_unused]]
static std::shared_ptr<RedisCacheClient> testCacheClient(EventLoop *pLoop, const InetAddress &serverAddr) {
    const auto client = std::make_shared<RedisCacheClient>(pLoop, serverAddr);

    client->start();

    client->trackKey(
        "k2",
        [pClient = client.get()](const std::string &key, const std::vector<char> &data) {
            MINI_MUDUO_LOG_DEBUG("Get {} = {}", key, std::string{data.data(), data.size()});

            static std::once_flag setOnce;
            std::call_once(setOnce, [pClient, key] {
                std::string newValue = "500";
                pClient->setKey(key, std::vector<char>{newValue.c_str(), newValue.c_str() + newValue.length()});
            });
        },
        [](const std::string &key) {
            MINI_MUDUO_LOG_DEBUG("key {} was invalidated", key);
        });

    client->trackKey(
        "k3",
        [](const std::string &key, const std::vector<char> &data) {
            MINI_MUDUO_LOG_DEBUG("Get {} = {}", key, std::string{data.data(), data.size()});
        },
        [](const std::string &key) {
            MINI_MUDUO_LOG_DEBUG("key {} was invalidated", key);
        });

    return client;
}

int main() {
    EventLoop loop;
    const InetAddress serverAddr{"192.168.50.2", 6379};

    auto connections = testHiredisWrappers(&loop, serverAddr);

    for (const auto &c : connections) {
        c->connect();
    }

    auto cacheClient = testCacheClient(&loop, serverAddr);

    loop.loop();

    return 0;
}
