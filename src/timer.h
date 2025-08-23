#ifndef MINI_MUDUO_TIMER_H
#define MINI_MUDUO_TIMER_H

#include <chrono>

#include <mini_muduo/callbacks.h>
#include <mini_muduo/log.h>
#include <mini_muduo/timestamp.h>

namespace mini_muduo {

class Timer {
public:
    Timer(TimerCallback cb, Timestamp when, std::chrono::milliseconds interval)
        : cb_(std::move(cb))
        , expiration_(when)
        , interval_(interval)
        , repeat_(interval != std::chrono::milliseconds::zero()) {}

    ~Timer() = default;

    Timer(const Timer &other) = delete;
    Timer &operator=(const Timer &other) = delete;

    void run() const {
        cb_();
    }

    Timestamp expiration() const {
        return expiration_;
    }

    bool repeat() const {
        return repeat_;
    }

    void restart(Timestamp now) {
        if (repeat_) {
            expiration_ = addTime(now, interval_);
        } else {
            MINI_MUDUO_LOG_WARN("Should not restart a non-repeated timer");
            expiration_ = Timestamp::invalid();
        }
    }

private:
    const TimerCallback cb_;
    Timestamp expiration_;
    const std::chrono::milliseconds interval_;
    const bool repeat_;
};

}  // namespace mini_muduo

#endif
