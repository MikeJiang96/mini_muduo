#ifndef MINI_MUDUO_EVENT_LOOP_H
#define MINI_MUDUO_EVENT_LOOP_H

#include <unistd.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include <mini_muduo/callbacks.h>
#include <mini_muduo/timer_id.h>

namespace mini_muduo {

class Channel;
class EPoller;
class TimerQueue;

class EventLoop {
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop &other) = delete;
    EventLoop &operator=(const EventLoop &other) = delete;

    void loop();

    void quit();

    void runInLoop(Functor cb);

    void queueInLoop(Functor cb);

    // timers

    ///
    /// Runs callback at 'time'.
    /// Safe to call from other threads.
    ///
    TimerId runAt(Timestamp time, TimerCallback cb);
    ///
    /// Runs callback after @c delay seconds.
    /// Safe to call from other threads.
    ///
    TimerId runAfter(std::chrono::milliseconds delay, TimerCallback cb);
    ///
    /// Runs callback every @c interval seconds.
    /// Safe to call from other threads.
    ///
    TimerId runEvery(std::chrono::milliseconds interval, TimerCallback cb);
    ///
    /// Cancels the timer.
    /// Safe to call from other threads.
    ///
    void cancel(TimerId timerId);

    // Internal usage
    void wakeup() const;

    void updateChannel(Channel *pChannel);

    void removeChannel(Channel *pChannel);

    void assertInLoopThread() {
        if (!isInLoopThread()) {
            abortNotInLoopThread();
        }
    }

    bool isInLoopThread() const {
        return tid_ == gettid();
    }

private:
    static constexpr std::chrono::seconds kDefaultPollTimeout = std::chrono::seconds(10);

    void abortNotInLoopThread();

    void callPendingFunctors();

    const pid_t tid_ = gettid();

    bool looping_ = false;
    std::atomic<bool> quit_ = false;

    bool handlingEvents_ = false;
    bool callingPendingFunctors_ = false;

    // Pimpl
    const std::unique_ptr<EPoller> poller_;
    const std::unique_ptr<Channel> wakeupChannel_;
    const std::unique_ptr<TimerQueue> timerQueue_;

    std::mutex mu_;
    std::vector<Functor> pendingFunctors_;
};

}  // namespace mini_muduo

#endif
