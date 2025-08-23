#include <mini_muduo/event_loop.h>

#include <sys/time.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <cassert>
#include <cstring>
#include <ctime>
#include <iostream>
#include <thread>

#include <mini_muduo/channel.h>
#include <mini_muduo/timestamp.h>

using namespace mini_muduo;

static EventLoop *l_pLoop;

static void timeout(int timerFd, Timestamp receiveTime) {
    std::cout << "timerFd = " << timerFd << ", timeout at " << receiveTime.toString() << '\n';

    uint64_t howmany;
    ssize_t n = ::read(timerFd, &howmany, sizeof(howmany));

    assert(n == sizeof(howmany));
    (void)n;

    l_pLoop->runInLoop([] {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::cout << "After timeout\n";

        l_pLoop->quit();
    });
}

int main() {
    EventLoop loop;

    l_pLoop = &loop;

    const int timerFd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

    struct itimerspec howlong;
    memset(&howlong, 0, sizeof(howlong));

    howlong.it_value.tv_sec = 2;
    ::timerfd_settime(timerFd, 0, &howlong, nullptr);

    Channel timerChannel(&loop, timerFd);

    timerChannel.setReadCallback([timerFd](Timestamp receiveTime) {
        timeout(timerFd, receiveTime);
    });

    timerChannel.enableReading();

    loop.runInLoop([] {
        std::cout << "Before timeout\n";
    });

    loop.loop();

    timerChannel.disableAll();
    timerChannel.remove();
    ::close(timerFd);

    return 0;
}
