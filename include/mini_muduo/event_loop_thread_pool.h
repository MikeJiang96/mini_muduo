#ifndef MINI_MUDUO_EVENT_LOOP_THREAD_POOL_H
#define MINI_MUDUO_EVENT_LOOP_THREAD_POOL_H

#include <memory>
#include <string>
#include <vector>

#include <mini_muduo/event_loop_thread.h>

namespace mini_muduo {

class EventLoopThreadPool {
public:
    EventLoopThreadPool(EventLoop *pMainLoop, int nThreads)
        : pMainLoop_(pMainLoop)
        , nThreads_(nThreads) {}

    ~EventLoopThreadPool() = default;

    EventLoopThreadPool(const EventLoopThreadPool &other) = delete;
    EventLoopThreadPool &operator=(const EventLoopThreadPool &other) = delete;

    void start() {
        pMainLoop_->assertInLoopThread();

        for (int i = 0; i < nThreads_; i++) {
            const std::string threadName = std::string("EventLoopThread#") + std::to_string(i);

            threads_.push_back(std::make_unique<EventLoopThread>(threadName));

            loops_.push_back(threads_[static_cast<size_t>(i)]->startLoop());
        }
    }

    EventLoop *getNextLoop() {
        if (nThreads_ == 0) {
            return pMainLoop_;
        }

        EventLoop *ret = loops_[static_cast<size_t>(next_)];

        if (++next_ >= nThreads_) {
            next_ = 0;
        }

        return ret;
    }

private:
    EventLoop *pMainLoop_;
    int nThreads_;

    int next_ = 0;

    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop *> loops_;
};

}  // namespace mini_muduo

#endif
