#ifndef MINI_MUDUO_TIMESTAMP_H
#define MINI_MUDUO_TIMESTAMP_H

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

namespace mini_muduo {

class Timestamp {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    static constexpr int kMicroSecondsPerSecond = 1000 * 1000;

    Timestamp() = default;

    explicit Timestamp(TimePoint tp)
        : timePoint_(tp) {}

    TimePoint timePoint() const {
        return timePoint_;
    }

    std::chrono::seconds secondsSinceEpoch() const {
        return std::chrono::duration_cast<std::chrono::seconds>(timePoint_.time_since_epoch());
    }

    std::chrono::milliseconds milliSecondsSinceEpoch() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(timePoint_.time_since_epoch());
    }

    std::chrono::microseconds microSecondsSinceEpoch() const {
        return std::chrono::duration_cast<std::chrono::microseconds>(timePoint_.time_since_epoch());
    }

    bool valid() const {
        return timePoint_ != invalid().timePoint_;
    }

    std::string toString() const {
        // Map steady_clock time to system_clock using current offsets
        const auto nowSystem = std::chrono::system_clock::now();
        const auto nowSteady = std::chrono::steady_clock::now();
        const auto sysTp = nowSystem + (timePoint_ - nowSteady);

        // Break down into seconds + milliseconds
        const auto secs = std::chrono::time_point_cast<std::chrono::seconds>(sysTp);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(sysTp - secs).count();

        // Convert to struct tm
        const auto timeT = std::chrono::system_clock::to_time_t(sysTp);
        struct tm tm;

        ::localtime_r(&timeT, &tm);

        std::ostringstream oss;

        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        oss << '.' << std::setfill('0') << std::setw(3) << ms;

        return oss.str();
    }

    static Timestamp now() {
        return Timestamp(Clock::now());
    }

    static Timestamp invalid() {
        return Timestamp{};
    }

    bool operator<(const Timestamp &other) const {
        return timePoint_ < other.timePoint_;
    }

    bool operator==(const Timestamp &other) const {
        return timePoint_ == other.timePoint_;
    }

private:
    TimePoint timePoint_{};
};

///
/// Add @c seconds to given timestamp.
///
/// @return timestamp+ms as Timestamp
///
inline Timestamp addTime(Timestamp timestamp, std::chrono::milliseconds ms) {
    return Timestamp(timestamp.timePoint() + ms);
}

}  // namespace mini_muduo

#endif
