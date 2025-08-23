#include <hiredis_demo/redis_cache_client.h>

#include <cassert>
#include <chrono>
#include <mutex>

#include <hiredis/hiredis.h>
#include <hiredis/read.h>

#include <mini_muduo/event_loop.h>
#include <mini_muduo/log.h>

namespace mini_muduo {

int RedisCacheClient::start() {
    int ret = REDIS_OK;

    std::call_once(startOnce_, [this, &ret] {
        if (cacheConn_.connect() != REDIS_OK || writeConn_.connect() != REDIS_OK) {
            ret = REDIS_ERR;
            return;
        }

        cacheConn_.setConnectCallback([this] {
            this->cacheConn_.command(
                [this](redisReply *reply) {
                    if (!reply) {
                        return;
                    }

                    this->onHello3(reply);
                },
                "HELLO 3");
        });

        cacheConn_.setDisconnectCallback([this](int status) {
            if (this->disconnectCb_) {
                this->disconnectCb_(status);
            }
        });
    });

    return ret;
}

void RedisCacheClient::onHello3(redisReply *reply) {
    (void)reply;

    auto trackKeyCb = [this](redisReply *reply) {
        if (!reply) {
            return;
        }

        if (this->cacheConn_.checkReply()) {
            if (reply->type != REDIS_REPLY_STRING && ::strcmp(reply->str, "OK") != 0) {
                return;
            }
        }

        cacheConn_.setAsyncPushCallBack([this](redisReply *reply) {
            this->onPush(reply);
        });

        {
            std::lock_guard lg{this->mu_};
            this->inited_ = true;
            this->cond_.notify_all();
        }
    };

    cacheConn_.command(std::move(trackKeyCb), "CLIENT TRACKING ON");
}

void RedisCacheClient::onPush(redisReply *reply) {
    if (!reply) {
        return;
    }

    if (cacheConn_.checkReply()) {
        if (reply->type != REDIS_REPLY_PUSH && reply->element[0]->type != REDIS_REPLY_STRING &&
            ::strcmp(reply->element[0]->str, "invalidate") != 0) {
            MINI_MUDUO_LOG_ERROR("Invalid reply - {} {}", reply->type);
            return;
        }
    }

    const auto element = reply->element[1]->element;
    const auto elements = reply->element[1]->elements;

    for (int i = 0; i < static_cast<int>(elements); i++) {
        const auto invalidatedKey = std::string{element[i]->str, element[i]->len};

        assert(getKeyCbs_.count(invalidatedKey));
        assert(keyInvalidatedCbs_.count(invalidatedKey));

        keyInvalidatedCbs_[invalidatedKey](invalidatedKey);

        cacheConn_.commandWithPointerToCallback(
            &getKeyCbs_[invalidatedKey], "GET %b", invalidatedKey.c_str(), invalidatedKey.length());
    }
}

void RedisCacheClient::trackKey(const std::string &key,
                                GetKeyCallback userGetKeyCb,
                                KeyInvalidatedCallback keyInvalidatedCb) {
    if (!userGetKeyCb || !keyInvalidatedCb) {
        MINI_MUDUO_LOG_ERROR("Must provide none empty cb");
        return;
    }

    if (getKeyCbs_.count(key)) {
        MINI_MUDUO_LOG_ERROR("Already tracking {}", key);
        return;
    }

    assert(getKeyCbs_.size() == keyInvalidatedCbs_.size());

    getKeyCbs_[key] = [this, key, userGetKeyCb = std::move(userGetKeyCb)](redisReply *reply) {
        if (!reply) {
            return;
        }

        if (reply->type == REDIS_REPLY_NIL) {
            MINI_MUDUO_LOG_WARN("key {} not exists", key);
            userGetKeyCb(key, {});
            return;
        }

        if (this->cacheConn_.checkReply()) {
            if (reply->type != REDIS_REPLY_STRING) {
                return;
            }
        }

        userGetKeyCb(key, std::vector<char>{reply->str, reply->str + reply->len});
    };

    keyInvalidatedCbs_[key] = std::move(keyInvalidatedCb);

    auto getKeyAfterInitedFunc = [this, key] {
        {
            std::unique_lock lg{mu_};

            if (!cond_.wait_for(lg, std::chrono::milliseconds(1), [this] {
                    return this->inited_;
                })) {
                return;
            }
        }

        (void)cacheConn_.commandWithPointerToCallback(&getKeyCbs_[key], "GET %b", key.c_str(), key.length());

        assert(this->regTrackTimers_.count(key));

        this->cacheConn_.getLoop()->cancel(this->regTrackTimers_[key]);
        this->regTrackTimers_.erase(key);
    };

    regTrackTimers_[key] =
        cacheConn_.getLoop()->runEvery(std::chrono::milliseconds(100), std::move(getKeyAfterInitedFunc));
}

int RedisCacheClient::setKey(const std::string &key,
                             const std::vector<char> value,
                             SetCompleteCallback cb,
                             std::chrono::seconds ttl) {
    return writeConn_.setKey(
        key,
        value,
        [key, cb = std::move(cb)](redisReply *) {
            MINI_MUDUO_LOG_DEBUG("key {} set done", key);

            if (cb) {
                cb(std::move(key));
            }
        },
        ttl);
}

}  // namespace mini_muduo
