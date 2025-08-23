#include <hiredis_demo/hiredis_wrapper.h>

#include <cassert>
#include <chrono>
#include <memory>

#include <hiredis/hiredis.h>
#include <hiredis/read.h>

#include <mini_muduo/channel.h>
#include <mini_muduo/event_loop.h>
#include <mini_muduo/log.h>
#include <mini_muduo/socket_ops.h>

namespace mini_muduo {

#define CHECK_CONNECTED()                         \
    do {                                          \
        if (!connected()) {                       \
            MINI_MUDUO_LOG_ERROR("!connected()"); \
            return REDIS_ERR;                     \
        }                                         \
    } while (0)

#define CHECK_COMMAND_CSTRING(cstr)                            \
    do {                                                       \
        if (!cstr || cstr[0] == '\0') {                        \
            MINI_MUDUO_LOG_ERROR("Must provide a valid cstr"); \
            return REDIS_ERR;                                  \
        }                                                      \
    } while (0)

#define CHECK_COMMAND_STRING(str)                             \
    do {                                                      \
        if (str.empty()) {                                    \
            MINI_MUDUO_LOG_ERROR("Must provide a valid str"); \
            return REDIS_ERR;                                 \
        }                                                     \
    } while (0)

#define CHECK_COMMAND_PCB(pcb)                                    \
    do {                                                          \
        if (!pcb) {                                               \
            MINI_MUDUO_LOG_ERROR("Must provide a none null pcb"); \
            return REDIS_ERR;                                     \
        }                                                         \
    } while (0)

#define CHECK_COMMAND_CB(cb)                                      \
    do {                                                          \
        if (!cb) {                                                \
            MINI_MUDUO_LOG_ERROR("Must provide a none empty cb"); \
            return REDIS_ERR;                                     \
        }                                                         \
    } while (0)

static void printRedisReplyIndented(const redisReply *reply, int indent) {
    if (reply == NULL) {
        printf("%*s(NULL)\n", indent, "");
        return;
    }

    switch (reply->type) {
        case REDIS_REPLY_STRING:
        case REDIS_REPLY_STATUS:
        case REDIS_REPLY_ERROR:
            printf("%*s", indent, "");
            putchar('"');
            for (size_t i = 0; i < reply->len; i++) {
                switch (reply->str[i]) {
                    case '\\':
                        printf("\\\\");
                        break;
                    case '"':
                        printf("\\\"");
                        break;
                    case '\n':
                        printf("\\n");
                        break;
                    case '\r':
                        printf("\\r");
                        break;
                    case '\t':
                        printf("\\t");
                        break;
                    default:
                        putchar(reply->str[i]);
                }
            }
            printf("\"\n");
            break;
        case REDIS_REPLY_INTEGER:
            printf("%*s%lld\n", indent, "", reply->integer);
            break;
        case REDIS_REPLY_ARRAY:
            printf("%*s[\n", indent, "");
            for (size_t i = 0; i < reply->elements; i++) {
                printRedisReplyIndented(reply->element[i], indent + 2);
            }
            printf("%*s]\n", indent, "");
            break;
        case REDIS_REPLY_PUSH:
            printf("%*s(push) [\n", indent, "");
            for (size_t i = 0; i < reply->elements; i++) {
                printRedisReplyIndented(reply->element[i], indent + 2);
            }
            printf("%*s]\n", indent, "");
            break;
        case REDIS_REPLY_NIL:
            printf("%*s(nil)\n", indent, "");
            break;
        default:
            printf("%*sUnknown type: %d\n", indent, "", reply->type);
    }
}

void printRedisReply(const redisReply *reply) {
    printRedisReplyIndented(reply, 0);
}

HiredisWrapper::HiredisWrapper(EventLoop *pLoop, const InetAddress &serverAddr)
    : pOwerLoop_(pLoop)
    , serverAddr_(serverAddr) {}

HiredisWrapper::~HiredisWrapper() {
    assert(!channel_ || channel_->isNoneEvent());

    ::redisAsyncFree(context_);
}

int HiredisWrapper::connect() {
    assert(!context_);

    context_ = ::redisAsyncConnect(serverAddr_.toIp().c_str(), serverAddr_.port());
    if (!context_ || context_->err) {
        if (context_ && context_->err) {
            MINI_MUDUO_LOG_ERROR("redisAsyncConnect error - {}", context_->errstr);
        }

        redisAsyncFree(context_);
        context_ = nullptr;

        return REDIS_ERR;
    }

    context_->ev.addRead = addRead;
    context_->ev.delRead = delRead;
    context_->ev.addWrite = addWrite;
    context_->ev.delWrite = delWrite;
    context_->ev.cleanup = cleanup;
    context_->ev.data = this;

    setChannel();

    assert(context_->onConnect == nullptr);
    assert(context_->onDisconnect == nullptr);

    ::redisAsyncSetConnectCallback(context_, connectCallback);
    ::redisAsyncSetDisconnectCallback(context_, disconnectCallback);

    return REDIS_OK;
}

void HiredisWrapper::setChannel() {
    assert(!channel_);

    channel_ = std::make_unique<Channel>(pOwerLoop_, fd());

    channel_->setReadCallback([this](Timestamp receiveTime) {
        this->handleRead(receiveTime);
    });

    channel_->setWriteCallback([this] {
        this->handleWrite();
    });
}

int HiredisWrapper::fd() const {
    assert(context_);
    return context_->c.fd;
}

void HiredisWrapper::removeAndResetChannel() {
    channel_->disableAll();
    channel_->remove();

    pOwerLoop_->queueInLoop([this] {
        this->resetChannel();
    });
}

void HiredisWrapper::resetChannel() {
    channel_.reset();
}

void HiredisWrapper::disconnect() {
    if (connected()) {
        ::redisAsyncDisconnect(context_);
    }
}

bool HiredisWrapper::connected() const {
    return channel_ && context_ && (context_->c.flags & REDIS_CONNECTED);
}

void HiredisWrapper::handleRead(Timestamp receiveTime) {
    (void)receiveTime;

    MINI_MUDUO_LOG_TRACE("receiveTime = {}", receiveTime.toString());

    ::redisAsyncHandleRead(context_);
}

void HiredisWrapper::handleWrite() {
    ::redisAsyncHandleWrite(context_);
}

/* static */ HiredisWrapper *HiredisWrapper::getHiredisWrapper(const redisAsyncContext *ac) {
    auto self = static_cast<HiredisWrapper *>(ac->ev.data);

    assert(self->context_ == ac);

    return self;
}

/* static */ void HiredisWrapper::connectCallback(const redisAsyncContext *ac, int status) {
    getHiredisWrapper(ac)->onConnect(status);
}

void HiredisWrapper::onConnect(int status) {
    if (status != REDIS_OK) {
        MINI_MUDUO_LOG_ERROR("failed to connect to {} - {}", serverAddr_.toIpPort(), context_->errstr);

        // TODO: back-off retry like mini_muduo::Connector
        return;
    } else {
        logConnection(true);
    }

    if (connectCb_) {
        connectCb_();
    }
}

void HiredisWrapper::logConnection(bool up) const {
    InetAddress localAddr(socket_ops::getLocalAddr(fd()));
    InetAddress peerAddr(socket_ops::getPeerAddr(fd()));

    MINI_MUDUO_LOG_INFO("{} -> {} is {}", localAddr.toIpPort(), peerAddr.toIpPort(), up ? "UP" : "DOWN");
}

/* static */ void HiredisWrapper::disconnectCallback(const redisAsyncContext *ac, int status) {
    getHiredisWrapper(ac)->onDisconnect(status);
}

void HiredisWrapper::onDisconnect(int status) {
    logConnection(false);

    removeAndResetChannel();

    if (disconnectCb_) {
        disconnectCb_(status);
    }

    context_ = nullptr;
}

void HiredisWrapper::setAsyncPushCallBack(AsyncPushCallback cb) {
    asyncPushCb_ = std::move(cb);
    redisAsyncSetPushCallback(context_, asyncPushCallback);
}

/* static */ void HiredisWrapper::asyncPushCallback(redisAsyncContext *ac, void *reply) {
    auto self = static_cast<HiredisWrapper *>(getHiredisWrapper(ac));
    auto pReply = static_cast<redisReply *>(reply);

    self->asyncPushCb_(pReply);
}

int HiredisWrapper::command(CommandCallback cb, const char *cmd, ...) {
    CHECK_CONNECTED();
    CHECK_COMMAND_CB(cb);
    CHECK_COMMAND_CSTRING(cmd);

    // May slightly leak a few *pCbs' memory if disconnect() happens before
    // remaining callback runs
    CommandCallback *pCb = new CommandCallback(std::move(cb));

    va_list ap;

    va_start(ap, cmd);
    const int ret = ::redisvAsyncCommand(context_, commandCallback, pCb, cmd, ap);
    va_end(ap);

    return ret;
}

/* static */ void HiredisWrapper::commandCallback(redisAsyncContext *ac, void *reply, void *privdata) {
    (void)ac;

    auto pReply = static_cast<redisReply *>(reply);
    auto pCb = static_cast<CommandCallback *>(privdata);

    assert(pCb && *pCb);

    (*pCb)(pReply);

    delete pCb;
}

int HiredisWrapper::commandWithPointerToCallback(CommandCallback *pCb, const char *cmd, ...) {
    CHECK_CONNECTED();
    CHECK_COMMAND_PCB(pCb);
    CHECK_COMMAND_CB(*pCb);
    CHECK_COMMAND_CSTRING(cmd);

    va_list ap;

    va_start(ap, cmd);
    const int ret = ::redisvAsyncCommand(context_, commandWithPointerToCallback, pCb, cmd, ap);
    va_end(ap);

    return ret;
}

/* static */ void HiredisWrapper::commandWithPointerToCallback(redisAsyncContext *ac, void *reply, void *privdata) {
    (void)ac;
    auto pReply = static_cast<redisReply *>(reply);
    auto pCb = static_cast<CommandCallback *>(privdata);

    assert(pCb);

    (*pCb)(pReply);
}

/* static */ void HiredisWrapper::addRead(void *privdata) {
    const auto *self = static_cast<HiredisWrapper *>(privdata);
    self->channel_->enableReading();
}

/* static */ void HiredisWrapper::delRead(void *privdata) {
    const auto *self = static_cast<HiredisWrapper *>(privdata);
    self->channel_->disableReading();
}

/* static */ void HiredisWrapper::addWrite(void *privdata) {
    const auto *self = static_cast<HiredisWrapper *>(privdata);
    self->channel_->enableWriting();
}

/* static */ void HiredisWrapper::delWrite(void *privdata) {
    const auto *self = static_cast<HiredisWrapper *>(privdata);
    self->channel_->disableWriting();
}

/* static */ void HiredisWrapper::cleanup(void *privdata) {
    (void)privdata;
}

int HiredisWrapper::ping() {
    return command(
        [this](redisReply *reply) {
            this->pingCallback(reply);
        },
        "PING");
}

void HiredisWrapper::pingCallback(redisReply *reply) {
    (void)reply;

    MINI_MUDUO_LOG_DEBUG("{}", reply->str);
}

int HiredisWrapper::setKey(const std::string &key,
                           const std::vector<char> value,
                           CommandCallback cb,
                           std::chrono::seconds ttl) {
    auto setKeyCb = [this, key, cb = std::move(cb)](redisReply *reply) {
        if (!reply) {
            MINI_MUDUO_LOG_WARN("reply is nulltpr");
            return;
        }

        if (this->checkReply_) {
            if (reply->type != REDIS_REPLY_STRING && ::strcmp(reply->str, "OK") != 0) {
                MINI_MUDUO_LOG_ERROR("Invalid reply for SET {} - {}", key, reply->type);
                return;
            }
        }

        if (cb) {
            cb(reply);
        }
    };

    if (ttl > std::chrono::seconds(0)) {
        return command(
            std::move(setKeyCb), "SET %b %b EX %d", key.c_str(), key.length(), value.data(), value.size(), ttl.count());
    } else {
        return command(std::move(setKeyCb), "SET %b %b", key.c_str(), key.length(), value.data(), value.size());
    }
}

int HiredisWrapper::getKey(const std::string &key, CommandCallback cb) {
    CHECK_COMMAND_CB(cb);

    auto getKeyCb = [this, key, cb = std::move(cb)](redisReply *reply) {
        if (!reply) {
            MINI_MUDUO_LOG_WARN("reply is nulltpr");
            return;
        }

        if (this->checkReply_) {
            if (reply->type != REDIS_REPLY_NIL && reply->type != REDIS_REPLY_STRING) {
                return;
            }
        }

        cb(reply);
    };

    return command(std::move(getKeyCb), "GET %b", key.c_str(), key.length());
}

int HiredisWrapper::subscribe(const std::string &channelName, CommandCallback cb) {
    CHECK_COMMAND_CB(cb);

    if (subCmdCbs_.count(channelName)) {
        MINI_MUDUO_LOG_ERROR("Already subscribed to {}", channelName);
        return REDIS_ERR;
    }

    subCmdCbs_[channelName] = [this, channelName, cb = std::move(cb)](redisReply *reply) {
        if (!reply) {
            MINI_MUDUO_LOG_WARN("reply is nulltpr");
            return;
        }

        if (this->checkReply_) {
            constexpr int kReplyElementsSize = 3;

            if (reply->type != REDIS_REPLY_ARRAY) {
                MINI_MUDUO_LOG_ERROR("subscriber received not REDIS_REPLY_ARRAY, but {}", reply->type);
                return;
            }

            if (reply->elements != kReplyElementsSize) {
                MINI_MUDUO_LOG_ERROR("Invalid elements nums for subscribe - {}", reply->elements);
                return;
            }

            const auto element = reply->element;

            if (::strcmp(element[0]->str, "subscribe") == 0) {
                if (channelName != element[1]->str || element[2]->integer == 0) {
                    MINI_MUDUO_LOG_ERROR("subscribe reply not OK - {} {}", element[1]->str, element[2]->integer);
                    return;
                }
            } else if (::strcmp(element[0]->str, "message") == 0) {
                if (channelName != element[1]->str) {
                    MINI_MUDUO_LOG_ERROR("message reply not OK - {}", element[1]->str);
                    return;
                }
            } else {
                MINI_MUDUO_LOG_ERROR("Unknown reply - {}", element[0]->str);
                return;
            }
        }

        cb(reply);
    };

    return commandWithPointerToCallback(&subCmdCbs_[channelName], "SUBSCRIBE %s", channelName.c_str());
}

}  // namespace mini_muduo
