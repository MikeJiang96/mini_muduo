#include "timer_queue.h"

#include <sys/timerfd.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>

#include <mini_muduo/event_loop.h>
#include <mini_muduo/log.h>

namespace mini_muduo {

static int createTimerFdOrDie() {
    const int timerFd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

    if (timerFd < 0) {
        MINI_MUDUO_LOG_CRITITAL("timerfd_create()");
        ::exit(EXIT_FAILURE);
    }

    return timerFd;
}

static struct timespec howMuchTimeFromNow(Timestamp when) {
    std::chrono::microseconds diff = when.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();

    if (diff.count() < 100) {
        diff = std::chrono::microseconds(100);
    }

    struct timespec ts;

    ts.tv_sec = static_cast<time_t>(diff.count() / Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>((diff.count() % Timestamp::kMicroSecondsPerSecond) * 1000);

    return ts;
}

static void readTimerFd(int timerFd, Timestamp now) {
    uint64_t howmany;
    ssize_t n = ::read(timerFd, &howmany, sizeof howmany);

    MINI_MUDUO_LOG_TRACE("TimerQueue::handleRead() {} at {}", howmany, now.toString());

    if (n != sizeof howmany) {
        MINI_MUDUO_LOG_ERROR("TimerQueue::handleRead() reads {} bytes instead of 8", n);
    }
}

static void resetTimerFd(int timerFd, Timestamp expiration) {
    // wake up loop by timerfd_settime()
    struct itimerspec newValue;
    struct itimerspec oldValue;

    memset(&newValue, 0, sizeof(newValue));
    memset(&oldValue, 0, sizeof(oldValue));

    newValue.it_value = howMuchTimeFromNow(expiration);

    int ret = ::timerfd_settime(timerFd, 0, &newValue, &oldValue);

    if (ret) {
        MINI_MUDUO_LOG_ERROR("timerfd_settime()");
    }
}

TimerQueue::TimerQueue(EventLoop *pLoop)
    : pOwnerLoop_(pLoop)
    , timerFd_(createTimerFdOrDie())
    , timerFdChannel_(pLoop, timerFd_) {
    timerFdChannel_.setReadCallback([this](Timestamp receiveTime) {
        (void)receiveTime;
        this->handleRead();
    });

    // we are always reading the timerFd, we disarm it with timerfd_settime.
    timerFdChannel_.enableReading();
}

TimerQueue::~TimerQueue() {
    timerFdChannel_.disableAll();
    timerFdChannel_.remove();

    ::close(timerFd_);
}

TimerId TimerQueue::addTimer(TimerCallback cb, Timestamp when, std::chrono::milliseconds interval) {
    const auto timer = std::make_shared<Timer>(std::move(cb), when, interval);

    pOwnerLoop_->runInLoop([this, timer] {
        this->addTimerInLoop(timer);
    });

    return TimerId(timer);
}

void TimerQueue::cancel(TimerId timerId) {
    pOwnerLoop_->runInLoop([this, timerId]() {
        this->cancelInLoop(timerId);
    });
}

void TimerQueue::addTimerInLoop(std::shared_ptr<Timer> timer) {
    pOwnerLoop_->assertInLoopThread();

    const bool earliestChanged = insert(timer);

    if (earliestChanged) {
        resetTimerFd(timerFd_, timer->expiration());
    }
}

void TimerQueue::cancelInLoop(TimerId timerId) {
    pOwnerLoop_->assertInLoopThread();

    const auto timer = timerId.pTimer_.lock();

    if (!timer) {
        MINI_MUDUO_LOG_WARN("Already canceled timer");
        return;
    }

    timers_.erase(Entry{timer->expiration(), timer});

    if (callingExpiredTimers_) {
        cancelingTimers_.insert(timer);
    }
}

void TimerQueue::handleRead() {
    pOwnerLoop_->assertInLoopThread();

    const auto now = Timestamp::now();

    readTimerFd(timerFd_, now);

    const auto expiredEntries = getExpiredEntries(now);

    callingExpiredTimers_ = true;

    for (const auto &expiredEntry : expiredEntries) {
        expiredEntry.second->run();
    }

    callingExpiredTimers_ = false;

    reset(expiredEntries, now);

    cancelingTimers_.clear();
}

std::vector<TimerQueue::Entry> TimerQueue::getExpiredEntries(Timestamp now) {
    Entry sentry{now, std::shared_ptr<Timer>(reinterpret_cast<Timer *>(UINTPTR_MAX), [](Timer *pTimer) {
                     // Sentinel, not a real object
                     (void)pTimer;
                 })};

    const auto expiredEnd = timers_.lower_bound(sentry);

    // User space pointer can not be UINTPTR_MAX, so we assert using '<'
    assert(expiredEnd == timers_.end() || now < expiredEnd->first);

    std::vector<Entry> ret;

    std::copy(timers_.begin(), expiredEnd, back_inserter(ret));

    // move out all expired timers
    timers_.erase(timers_.begin(), expiredEnd);

    return ret;
}

void TimerQueue::reset(const std::vector<Entry> &expiredEntries, Timestamp now) {
    for (const auto &expiredEntry : expiredEntries) {
        const bool repeat = expiredEntry.second->repeat();
        const bool canceled = cancelingTimers_.count(expiredEntry.second);

        if (repeat && !canceled) {
            expiredEntry.second->restart(now);
            (void)insert(expiredEntry.second);
        }
    }

    Timestamp nextExpire;

    if (!timers_.empty()) {
        nextExpire = timers_.begin()->second->expiration();
    }

    if (nextExpire.valid()) {
        resetTimerFd(timerFd_, nextExpire);
    }
}

bool TimerQueue::insert(const std::shared_ptr<Timer> &timer) {
    pOwnerLoop_->assertInLoopThread();

    bool earliestChanged = false;

    const Timestamp when = timer->expiration();

    if (timers_.empty() || when < timers_.begin()->first) {
        earliestChanged = true;
    }

    {
        const auto [_, succ] = timers_.insert(Entry{when, timer});
        assert(succ);
        (void)succ;
    }

    return earliestChanged;
}

}  // namespace mini_muduo
