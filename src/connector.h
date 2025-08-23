#ifndef MINI_MUDUO_CONNNECTOR_H
#define MINI_MUDUO_CONNNECTOR_H

#include <chrono>
#include <functional>
#include <memory>
#include <utility>

#include <mini_muduo/channel.h>
#include <mini_muduo/event_loop.h>
#include <mini_muduo/inet_address.h>

namespace mini_muduo {

class Connector : public std::enable_shared_from_this<Connector> {
public:
    using NewConnectionCallback = std::function<void(int sockFd)>;

    Connector(EventLoop *pLoop, const InetAddress &serverAddr)
        : pOwnerLoop_(pLoop)
        , serverAddr_(serverAddr) {}

    ~Connector() = default;

    Connector(const Connector &other) = delete;
    Connector &operator=(const Connector &other) = delete;

    void setNewConnectionCallback(NewConnectionCallback cb) {
        newConnectionCallback_ = std::move(cb);
    }

    const InetAddress &serverAddress() const {
        return serverAddr_;
    }

    void start();    // can be called in any thread
    void stop();     // can be called in any thread
    void restart();  // must be called in loop thread

private:
    static constexpr auto kMaxRetryDelay = std::chrono::milliseconds(30 * 1000);
    static constexpr auto kInitRetryDelay = std::chrono::milliseconds(500);

    enum class State {
        DISCONNECTED,
        CONNECTING,
        CONNECTED
    };

    void setState(State state) {
        state_.store(state);
    }

    void startInLoop();
    void stopInLoop();

    void connect();
    void connecting(int sockFd);
    void retry(int sockFd);

    int removeAndResetChannel();
    void resetChannel();

    void handleWrite();
    void handleError();

    EventLoop *pOwnerLoop_;
    const InetAddress serverAddr_;

    std::atomic<State> state_ = State::DISCONNECTED;
    std::atomic<bool> connect_ = false;

    std::chrono::milliseconds retryDelay_ = kInitRetryDelay;

    std::unique_ptr<Channel> channel_;
    NewConnectionCallback newConnectionCallback_;
};

}  // namespace mini_muduo

#endif
