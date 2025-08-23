#include <mini_muduo/channel.h>

#include <mini_muduo/event_loop.h>

namespace mini_muduo {

void Channel::handleEvents(Timestamp receiveTime) {
    handlingEvents_ = false;

    if ((receivedEvents_ & EPOLLHUP) && !(receivedEvents_ & EPOLLIN)) {
        if (closeCb_) {
            closeCb_();
        }
    }

    if (receivedEvents_ & EPOLLERR) {
        if (errorCb_) {
            errorCb_();
        }
    }

    if (receivedEvents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
        if (readCb_) {
            readCb_(receiveTime);
        }
    }

    if (receivedEvents_ & EPOLLOUT) {
        if (writeCb_)
            writeCb_();
    }

    handlingEvents_ = false;
}

void Channel::remove() {
    addedToLoop_ = false;
    pOwnerLoop_->removeChannel(this);
}

void Channel::update() {
    addedToLoop_ = true;
    pOwnerLoop_->updateChannel(this);
}

}  // namespace mini_muduo
