#ifndef MINI_MUDUO_TIMER_QUEUE_H
#define MINI_MUDUO_TIMER_QUEUE_H

#include <chrono>
#include <memory>
#include <set>
#include <utility>

#include "timer.h"

#include <mini_muduo/channel.h>
#include <mini_muduo/timer_id.h>
#include <mini_muduo/timestamp.h>

namespace mini_muduo {

class TimerQueue {
public:
    explicit TimerQueue(EventLoop *oLoop);
    ~TimerQueue();

    TimerQueue(const TimerQueue &other) = delete;
    TimerQueue &operator=(const TimerQueue &other) = delete;

    TimerId addTimer(TimerCallback cb, Timestamp when, std::chrono::milliseconds interval);

    void cancel(TimerId timerId);

private:
    // Use std::shared_ptr to simplify implementation
    using Entry = std::pair<Timestamp, std::shared_ptr<Timer>>;
    using TimerList = std::set<Entry>;

    void addTimerInLoop(std::shared_ptr<Timer> timer);
    void cancelInLoop(TimerId timerId);

    // called when timerFd_ alarms, not using epoll's timestamp.
    void handleRead();

    // move out all expired timers
    std::vector<Entry> getExpiredEntries(Timestamp now);

    void reset(const std::vector<Entry> &expiredEntries, Timestamp now);

    bool insert(const std::shared_ptr<Timer> &timer);

    EventLoop *pOwnerLoop_;
    const int timerFd_;
    Channel timerFdChannel_;

    // Timer list sorted by expiration
    TimerList timers_;

    // for cancel()
    bool callingExpiredTimers_;
    std::set<std::shared_ptr<Timer>> cancelingTimers_;
};

}  // namespace mini_muduo

#endif
