#include <stdio.h>
#include <unistd.h>

#include <chrono>

#include <mini_muduo/event_loop.h>

using namespace mini_muduo;

int cnt = 0;
EventLoop *g_loop;

void printTid() {
    printf("pid = %d, tid = %d\n", getpid(), gettid());
    printf("now %s\n", Timestamp::now().toString().c_str());
}

void print(const char *msg) {
    printf("msg %s %s\n", Timestamp::now().toString().c_str(), msg);
    if (++cnt == 20) {
        g_loop->quit();
    }
}

void cancel(TimerId timer) {
    g_loop->cancel(timer);
    printf("cancelled at %s\n", Timestamp::now().toString().c_str());
}

static TimerId l_toCancelSelf;

void cancelSelf() {
    printf("cancelSelf()\n");
    cancel(l_toCancelSelf);
}

int main() {
    printTid();
    sleep(1);
    {
        EventLoop loop;
        g_loop = &loop;

        print("main");
        loop.runAfter(std::chrono::seconds(1), std::bind(print, "once1"));
        loop.runAfter(std::chrono::milliseconds(1500), std::bind(print, "once1.5"));
        loop.runAfter(std::chrono::milliseconds(2500), std::bind(print, "once2.5"));
        loop.runAfter(std::chrono::milliseconds(3500), std::bind(print, "once3.5"));
        TimerId t45 = loop.runAfter(std::chrono::milliseconds(4500), std::bind(print, "once4.5"));
        loop.runAfter(std::chrono::milliseconds(4200), std::bind(cancel, t45));
        loop.runAfter(std::chrono::milliseconds(4800), std::bind(cancel, t45));
        loop.runEvery(std::chrono::seconds(2), std::bind(print, "every2"));
        TimerId t3 = loop.runEvery(std::chrono::seconds(3), std::bind(print, "every3"));
        loop.runAfter(std::chrono::milliseconds(9001), std::bind(cancel, t3));

        l_toCancelSelf = loop.runEvery(std::chrono::seconds(5), cancelSelf);

        loop.loop();
        print("main loop exits");
    }
    sleep(1);
    {
        EventLoop loop;

        loop.runAfter(std::chrono::seconds(2), printTid);

        sleep(3);
        print("thread loop exits");
    }
}
