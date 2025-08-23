#include <mini_muduo/event_loop.h>

#include <signal.h>
#include <sys/eventfd.h>

#include <cassert>
#include <chrono>
#include <cstdlib>

#include "epoller.h"
#include "timer_queue.h"

#include <mini_muduo/channel.h>
#include <mini_muduo/log.h>
#include <mini_muduo/timestamp.h>

namespace mini_muduo {

static int l_dummy = [] {
    ::signal(SIGPIPE, SIG_IGN);

    return 0;
}();

// One loop per thread guard
static thread_local EventLoop *t_LoopInThisThread = nullptr;

static int createEventFdOrDie() {
    const int eventFd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    if (eventFd < 0) {
        MINI_MUDUO_LOG_CRITITAL("eventfd()");
        ::exit(EXIT_FAILURE);
    }

    return eventFd;
}

static void readFromEventFd(int eventFd) {
    uint64_t dummy = 1;

    ssize_t n = ::read(eventFd, &dummy, sizeof(dummy));

    assert(n == sizeof(dummy));
    (void)n;
}

static void writeToEventFd(int eventFd) {
    uint64_t dummy = 1;

    ssize_t n = ::write(eventFd, &dummy, sizeof(dummy));

    assert(n == sizeof(dummy));
    (void)n;
}

EventLoop::EventLoop()
    : poller_(std::make_unique<EPoller>(this))
    , wakeupChannel_(std::make_unique<Channel>(this, createEventFdOrDie()))
    , timerQueue_(std::make_unique<TimerQueue>(this)) {
    if (t_LoopInThisThread) {
        MINI_MUDUO_LOG_CRITITAL("Already created EventLoop in this thread");
        ::exit(EXIT_FAILURE);
    }

    t_LoopInThisThread = this;

    wakeupChannel_->setReadCallback([fd = wakeupChannel_->fd()](Timestamp receiveTime) {
        (void)receiveTime;
        readFromEventFd(fd);
    });

    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupChannel_->fd());

    t_LoopInThisThread = nullptr;
}

void EventLoop::loop() {
    assert(!looping_);
    assertInLoopThread();

    quit_.store(false);
    looping_ = true;

    while (!quit_) {
        auto [timeSinceEpoch, activeChannels] = poller_->poll(kDefaultPollTimeout);

        if (!activeChannels.empty()) {
            handlingEvents_ = true;

            for (Channel *pChannel : activeChannels) {
                pChannel->handleEvents(timeSinceEpoch);
            }

            handlingEvents_ = false;
        }

        callPendingFunctors();
    }

    looping_ = false;
}

void EventLoop::callPendingFunctors() {
    callingPendingFunctors_ = true;

    std::vector<Functor> functors;

    {
        std::lock_guard lg{mu_};
        functors.swap(pendingFunctors_);
    }

    for (const auto &functor : functors) {
        functor();
    }

    callingPendingFunctors_ = false;
}

void EventLoop::quit() {
    quit_.store(true);

    if (!isInLoopThread()) {
        wakeup();
    }
}

void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {
        cb();
    } else {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb) {
    {
        std::lock_guard lg{mu_};
        pendingFunctors_.push_back(std::move(cb));
    }

    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();
    }
}

TimerId EventLoop::runAt(Timestamp time, TimerCallback cb) {
    return timerQueue_->addTimer(std::move(cb), time, std::chrono::milliseconds::zero());
}

TimerId EventLoop::runAfter(std::chrono::milliseconds delay, TimerCallback cb) {
    Timestamp time(addTime(Timestamp::now(), delay));
    return runAt(time, std::move(cb));
}

TimerId EventLoop::runEvery(std::chrono::milliseconds interval, TimerCallback cb) {
    Timestamp time(addTime(Timestamp::now(), interval));
    return timerQueue_->addTimer(std::move(cb), time, interval);
}

void EventLoop::cancel(TimerId timerId) {
    return timerQueue_->cancel(timerId);
}

void EventLoop::wakeup() const {
    writeToEventFd(wakeupChannel_->fd());
}

void EventLoop::updateChannel(Channel *pChannel) {
    assert(pChannel->ownerLoop() == this);
    assertInLoopThread();

    poller_->updateChannel(pChannel);
}

void EventLoop::removeChannel(Channel *pChannel) {
    assert(pChannel->ownerLoop() == this);
    assertInLoopThread();

    poller_->removeChannel(pChannel);
}

void EventLoop::abortNotInLoopThread() {
    MINI_MUDUO_LOG_CRITITAL("Must be in loop thread");
    ::exit(EXIT_FAILURE);
}

}  // namespace mini_muduo
