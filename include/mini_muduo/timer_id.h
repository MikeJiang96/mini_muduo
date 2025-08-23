#ifndef MINI_MUDUO_TIMER_ID_H
#define MINI_MUDUO_TIMER_ID_H

#include <memory>

namespace mini_muduo {

class Timer;

class TimerId {
    friend class TimerQueue;

public:
    TimerId() = default;

private:
    explicit TimerId(std::weak_ptr<Timer> pTimer)
        : pTimer_(pTimer) {}

    std::weak_ptr<Timer> pTimer_;
};

}  // namespace mini_muduo

#endif
