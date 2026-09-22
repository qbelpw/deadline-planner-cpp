#include "deadline/date_time.hpp"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "deadline/common.hpp"

namespace deadline {
namespace {

std::tm localTime(const std::time_t time) noexcept {
    std::tm result{};
#ifdef _WIN32
    localtime_s(&result, &time);
#else
    localtime_r(&time, &result);
#endif
    return result;
}

std::string durationLabel(const std::int64_t minutes) {
    const auto absolute = std::max<std::int64_t>(0, minutes);
    const auto days = absolute / (24 * 60);
    const auto hours = (absolute % (24 * 60)) / 60;
    const auto restMinutes = absolute % 60;
    std::ostringstream output;
    if (days > 0) {
        output << days << " д. ";
    }
    if (hours > 0 || days > 0) {
        output << hours << " ч. ";
    }
    output << restMinutes << " мин.";
    return output.str();
}

}  // namespace

DateTime::DateTime() noexcept
    : value_(Clock::now()) {}

DateTime::DateTime(const TimePoint value) noexcept
    : value_(value) {}

DateTime DateTime::now() noexcept {
    return DateTime(Clock::now());
}

DateTime DateTime::parse(const std::string& text) {
    std::tm parsed{};
    std::istringstream input(text);
    input >> std::get_time(&parsed, "%Y-%m-%d %H:%M");
    require(
        !input.fail() && input.peek() == std::char_traits<char>::eof(),
        "Дата должна иметь формат ГГГГ-ММ-ДД ЧЧ:ММ"
    );
    parsed.tm_isdst = -1;
    const auto value = std::mktime(&parsed);
    require(value != static_cast<std::time_t>(-1), "Некорректная дата");
    return DateTime(Clock::from_time_t(value));
}

std::string DateTime::format() const {
    const auto time = Clock::to_time_t(value_);
    const auto local = localTime(time);
    std::ostringstream output;
    output << std::put_time(&local, "%Y-%m-%d %H:%M");
    return output.str();
}

TimePoint DateTime::value() const noexcept {
    return value_;
}

DateTime DateTime::addDays(const int days) const noexcept {
    return DateTime(value_ + std::chrono::hours(24LL * days));
}

DateTime DateTime::addHours(const int hours) const noexcept {
    return DateTime(value_ + std::chrono::hours(hours));
}

std::int64_t DateTime::minutesUntil(const DateTime& other) const noexcept {
    return std::chrono::duration_cast<std::chrono::minutes>(
        other.value_ - value_
    ).count();
}

DeadlineScale DeadlineScale::calculate(
    const DateTime& periodStart,
    const DateTime& due,
    const DateTime& now,
    const bool completed
) {
    if (completed) {
        return DeadlineScale{
            1.0,
            0,
            "Выполнено",
            Urgency::completed
        };
    }

    const auto total = periodStart.minutesUntil(due);
    const auto elapsed = periodStart.minutesUntil(now);
    const auto remaining = now.minutesUntil(due);
    const double share = total <= 0
        ? 1.0
        : std::clamp(
            static_cast<double>(elapsed) / static_cast<double>(total),
            0.0,
            1.0
        );

    if (remaining < 0) {
        return DeadlineScale{
            1.0,
            remaining,
            "Просрочено на " + durationLabel(-remaining),
            Urgency::overdue
        };
    }
    if (remaining <= 24 * 60) {
        return DeadlineScale{
            share,
            remaining,
            "Осталось " + durationLabel(remaining),
            Urgency::urgent
        };
    }
    if (remaining <= 3 * 24 * 60) {
        return DeadlineScale{
            share,
            remaining,
            "Осталось " + durationLabel(remaining),
            Urgency::soon
        };
    }
    return DeadlineScale{
        share,
        remaining,
        "Осталось " + durationLabel(remaining),
        Urgency::normal
    };
}

}  // namespace deadline
