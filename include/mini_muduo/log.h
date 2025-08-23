#ifndef MINI_MUDUO_LOG_H
#define MINI_MUDUO_LOG_H

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include <spdlog/spdlog.h>

#define MINI_MUDUO_LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#define MINI_MUDUO_LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define MINI_MUDUO_LOG_INFO(...) SPDLOG_INFO(__VA_ARGS__)
#define MINI_MUDUO_LOG_WARN(...) SPDLOG_WARN(__VA_ARGS__)
#define MINI_MUDUO_LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
#define MINI_MUDUO_LOG_CRITITAL(...) SPDLOG_CRITICAL(__VA_ARGS__)

namespace mini_muduo {

inline void setLogLevel(spdlog::level::level_enum level) {
    spdlog::set_level(level);
}

const char *strerror_tl(int savedErrno);

}  // namespace mini_muduo

#endif
