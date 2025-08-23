#ifndef MINI_MUDUO_EPOLLER_H
#define MINI_MUDUO_EPOLLER_H

#include <sys/epoll.h>

#include <chrono>
#include <unordered_map>
#include <vector>

#include <mini_muduo/channel.h>
#include <mini_muduo/event_loop.h>
#include <mini_muduo/timestamp.h>

namespace mini_muduo {

struct PollRes {
    using ChannelList = std::vector<Channel *>;

    Timestamp receiveTime;
    ChannelList activeChannels;
};

class EPoller {
public:
    explicit EPoller(EventLoop *pLoop);
    ~EPoller();

    EPoller(const EPoller &other) = delete;
    EPoller &operator=(const EPoller &other) = delete;

    PollRes poll(std::chrono::milliseconds timeout);

    void updateChannel(Channel *pChannel);

    void removeChannel(Channel *pCannel);

private:
    using ChannelState = Channel::State;

    using ChannelHashMap = std::unordered_map<int, Channel *>;
    using EventList = std::vector<struct epoll_event>;

    static constexpr int kEventListInitSize = 16;

    PollRes::ChannelList getActiveChannels(int nEvents);

    void updateEventCtl(int operation, Channel *pChannel);

    EventLoop *pOwnerLoop_;
    const int epollFd_;

    ChannelHashMap channels_;

    // Memo size for performance
    EventList polledEvents_ = EventList(kEventListInitSize);
};

}  // namespace mini_muduo

#endif
