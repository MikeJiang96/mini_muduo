#ifndef MINI_MUDUO_EVENT_LOOP_THREAD_H
#define MINI_MUDUO_EVENT_LOOP_THREAD_H

#include <pthread.h>

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include <mini_muduo/event_loop.h>

namespace mini_muduo {

class EventLoopThread {
public:
    EventLoopThread(std::string name = {})
        : name_(std::move(name)) {}

    ~EventLoopThread() {
        if (pLoop_) {
            pLoop_->quit();

            std::this_thread::sleep_for(std::chrono::seconds(1));
            thread_.join();
        }
    }

    EventLoopThread(const EventLoopThread &other) = delete;
    EventLoopThread &operator=(const EventLoopThread &other) = delete;

    EventLoop *startLoop() {
        thread_ = std::thread([this] {
            this->threadFunc(this->name_);
        });

        {
            std::unique_lock lg{mu_};

            cond_.wait(lg, [this] {
                return pLoop_ != nullptr;
            });
        }

        return pLoop_;
    }

private:
    void threadFunc(const std::string &name) {
        (void)pthread_setname_np(pthread_self(), name.c_str());

        EventLoop loop;

        {
            std::lock_guard lg{mu_};
            pLoop_ = &loop;
            cond_.notify_one();
        }

        loop.loop();
    }

    const std::string name_;

    // Use atomic or lock in dtor???
    EventLoop *pLoop_ = nullptr;
    std::thread thread_;

    std::mutex mu_;
    std::condition_variable cond_;
};

}  // namespace mini_muduo

#endif
