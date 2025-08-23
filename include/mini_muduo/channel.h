#ifndef MINI_MUDUO_CHANNEL_H
#define MINI_MUDUO_CHANNEL_H

#include <sys/epoll.h>

#include <cassert>
#include <cstdint>
#include <functional>
#include <utility>

#include <mini_muduo/timestamp.h>

namespace mini_muduo {

class EventLoop;

class Channel {
public:
    // Used by EPoller
    enum class State {
        NEW,
        ADDED,
        IGNORED,
    };

    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *pLoop, int fd)
        : pOwnerLoop_(pLoop)
        , fd_(fd) {}

    ~Channel() {
        assert(!addedToLoop_);
        assert(!handlingEvents_);
    }

    Channel(const Channel &other) = delete;
    Channel &operator=(const Channel &other) = delete;

    int fd() const {
        return fd_;
    }

    State state() const {
        return state_;
    }

    void setState(State state) {
        state_ = state;
    }

    uint32_t concernedEvents() const {
        return concernedEvents_;
    }

    uint32_t receivedEvents() const {
        return receivedEvents_;
    }

    void setReceivedEvents(uint32_t receivedEvents) {
        receivedEvents_ = receivedEvents;
    }

    void setReadCallback(ReadEventCallback cb) {
        readCb_ = std::move(cb);
    }

    void setWriteCallback(EventCallback cb) {
        writeCb_ = std::move(cb);
    }

    void setCloseCallback(EventCallback cb) {
        closeCb_ = std::move(cb);
    }

    void setErrorCallback(EventCallback cb) {
        errorCb_ = std::move(cb);
    }

    void handleEvents(Timestamp receiveTime);

    void enableReading() {
        concernedEvents_ |= kReadEvent;
        update();
    }

    void enableWriting() {
        concernedEvents_ |= kWriteEvent;
        update();
    }

    void disableReading() {
        concernedEvents_ &= ~kReadEvent;
        update();
    }

    void disableWriting() {
        concernedEvents_ &= ~kWriteEvent;
        update();
    }

    void disableAll() {
        concernedEvents_ = kNoneEvent;
        update();
    }

    bool isReading() const {
        return concernedEvents_ & kReadEvent;
    }

    bool isWriting() const {
        return concernedEvents_ & kWriteEvent;
    }

    bool isNoneEvent() const {
        return concernedEvents_ == kNoneEvent;
    }

    EventLoop *ownerLoop() const {
        return pOwnerLoop_;
    }

    void remove();

private:
    static constexpr uint32_t kNoneEvent = 0;
    static constexpr uint32_t kReadEvent = EPOLLIN | EPOLLPRI;
    static constexpr uint32_t kWriteEvent = EPOLLOUT;

    void update();

    EventLoop *pOwnerLoop_;
    const int fd_;

    State state_ = State::NEW;

    bool addedToLoop_ = false;
    bool handlingEvents_ = false;

    uint32_t concernedEvents_ = 0;
    uint32_t receivedEvents_ = 0;

    ReadEventCallback readCb_;
    EventCallback writeCb_;
    EventCallback closeCb_;
    EventCallback errorCb_;
};

}  // namespace mini_muduo

#endif
