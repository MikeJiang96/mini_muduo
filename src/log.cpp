#include <mini_muduo/log.h>

#include <cstring>

namespace mini_muduo {

static const int l_dummy = [] {
    setLogLevel(spdlog::level::debug);

    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%t] [%l] [%s:%#] %v");

    return 0;
}();

const char *strerror_tl(int savedErrno) {
    static thread_local char t_errnobuf[512];

    return strerror_r(savedErrno, t_errnobuf, sizeof t_errnobuf);
}

}  // namespace mini_muduo
