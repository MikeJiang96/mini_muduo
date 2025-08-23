#include "epoller.h"

#include <sys/epoll.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cstring>

#include <mini_muduo/channel.h>
#include <mini_muduo/log.h>

namespace mini_muduo {

static int createEPollFd() {
    const int epollFd = ::epoll_create1(EPOLL_CLOEXEC);

    assert(epollFd >= 0);

    return epollFd;
}

EPoller::EPoller(EventLoop *pLoop)
    : pOwnerLoop_(pLoop)
    , epollFd_(createEPollFd()) {}

EPoller::~EPoller() {
    ::close(epollFd_);
}

PollRes EPoller::poll(std::chrono::milliseconds timeout) {
    PollRes res;

    const int nEvents = ::epoll_wait(
        epollFd_, polledEvents_.data(), static_cast<int>(polledEvents_.size()), static_cast<int>(timeout.count()));

    const int savedErrno = errno;

    res.receiveTime = Timestamp::now();

    if (nEvents > 0) {
        res.activeChannels = getActiveChannels(nEvents);

        if (polledEvents_.size() == static_cast<size_t>(nEvents)) {
            polledEvents_.resize(2 * polledEvents_.size());
        }
    } else if (nEvents == 0) {
        // Do nothing
    } else {
        if (savedErrno != EINTR) {
            errno = savedErrno;

            MINI_MUDUO_LOG_ERROR("epoll_wait() {}", strerror_tl(errno));
        }
    }

    return res;
}

PollRes::ChannelList EPoller::getActiveChannels(int nEvents) {
    PollRes::ChannelList res;

    for (int i = 0; i < nEvents; i++) {
        const auto &event = polledEvents_[static_cast<size_t>(i)];

        auto pChannel = channels_[event.data.fd];

        assert(pChannel->fd() == event.data.fd);

        pChannel->setReceivedEvents(event.events);

        res.push_back(pChannel);
    }

    return res;
}

void EPoller::updateChannel(Channel *pChannel) {
    pOwnerLoop_->assertInLoopThread();

    const int fd = pChannel->fd();
    const ChannelState channelState = pChannel->state();

    if (channelState == ChannelState::NEW || channelState == ChannelState::IGNORED) {
        if (channelState == ChannelState::NEW) {
            channels_[fd] = pChannel;
        }

        pChannel->setState(ChannelState::ADDED);

        updateEventCtl(EPOLL_CTL_ADD, pChannel);
    } else if (channelState == ChannelState::ADDED) {
        if (pChannel->isNoneEvent()) {
            pChannel->setState(ChannelState::IGNORED);

            updateEventCtl(EPOLL_CTL_DEL, pChannel);
        } else {
            updateEventCtl(EPOLL_CTL_MOD, pChannel);
        }
    } else {
        MINI_MUDUO_LOG_WARN("Invalid Channel state = {}", static_cast<int>(channelState));
    }
}

void EPoller::removeChannel(Channel *pChannel) {
    pOwnerLoop_->assertInLoopThread();

    const int fd = pChannel->fd();
    const ChannelState channelState = pChannel->state();

    assert(channelState == ChannelState::ADDED || channelState == ChannelState::IGNORED);

    const size_t n = channels_.erase(fd);
    assert(n == 1);
    (void)n;

    // Do not EPOLL_CTL_DEL twice if already IGNORED
    if (channelState == ChannelState::ADDED) {
        updateEventCtl(EPOLL_CTL_DEL, pChannel);
    }

    pChannel->setState(ChannelState::NEW);
}

void EPoller::updateEventCtl(int operation, Channel *pChannel) {
    const int fd = pChannel->fd();
    struct epoll_event event;

    memset(&event, 0, sizeof(event));

    event.events = pChannel->concernedEvents();
    event.data.fd = fd;

    if (::epoll_ctl(epollFd_, operation, fd, &event) != 0) {
        MINI_MUDUO_LOG_ERROR("epoll_ctl() fd = {}, events = {}", fd, event.events);
    }
}

}  // namespace mini_muduo
