#ifndef MINI_MUDUO_CALLBACKS_H
#define MINI_MUDUO_CALLBACKS_H

#include <cstddef>
#include <functional>
#include <memory>

#include <mini_muduo/timestamp.h>

namespace mini_muduo {

class TcpConnection;
class Buffer;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

using TimerCallback = std::function<void()>;

using ConnectionCallback = std::function<void(const TcpConnectionPtr &)>;

using MessageCallback = std::function<void(const TcpConnectionPtr &, Buffer &, Timestamp)>;

using WriteCompleteCallback = std::function<void(const TcpConnectionPtr &)>;

using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr &, size_t)>;

using CloseCallback = std::function<void(const TcpConnectionPtr &)>;

void defaultConnectionCallback(const TcpConnectionPtr &);
void defaultMessageCallback(const TcpConnectionPtr &, Buffer &, Timestamp);

}  // namespace mini_muduo

#endif
