#pragma once

#include <chrono>
#include <compare>
#include <cstdint>
#include <string>

#include "deadline/common.hpp"

namespace deadline {

using Clock = std::chrono::system_clock;
using TimePoint = Clock::time_point;

class TimeInterval final {
public:
    constexpr TimeInterval() noexcept = default;

    explicit TimeInterval(const std::int64_t minutes)
        : minutes_(minutes) {
        require(minutes >= 0, "Интервал времени не может быть отрицательным");
    }

    constexpr TimeInterval(const TimeInterval&) noexcept = default;
    constexpr TimeInterval(TimeInterval&&) noexcept = default;
    constexpr TimeInterval& operator=(
        const TimeInterval&
    ) noexcept = default;
    constexpr TimeInterval& operator=(
        TimeInterval&&
    ) noexcept = default;
    ~TimeInterval() = default;

    [[nodiscard]] constexpr std::int64_t minutes() const noexcept {
        return minutes_;
    }

    [[nodiscard]] TimeInterval operator+(
        const TimeInterval other
    ) const {
        return TimeInterval(minutes_ + other.minutes_);
    }

    TimeInterval& operator+=(const TimeInterval other) {
        minutes_ += other.minutes_;
        return *this;
    }

    auto operator<=>(const TimeInterval&) const noexcept = default;

private:
    std::int64_t minutes_ = 0;
};

class DateTime final {
public:
    DateTime() noexcept;
    explicit DateTime(TimePoint value) noexcept;
    DateTime(const DateTime&) noexcept = default;
    DateTime(DateTime&&) noexcept = default;
    DateTime& operator=(const DateTime&) noexcept = default;
    DateTime& operator=(DateTime&&) noexcept = default;
    ~DateTime() = default;

    [[nodiscard]] static DateTime now() noexcept;
    [[nodiscard]] static DateTime parse(const std::string& text);

    [[nodiscard]] std::string format() const;
    [[nodiscard]] TimePoint value() const noexcept;
    [[nodiscard]] DateTime addDays(int days) const noexcept;
    [[nodiscard]] DateTime addHours(int hours) const noexcept;
    [[nodiscard]] std::int64_t minutesUntil(
        const DateTime& other
    ) const noexcept;

    auto operator<=>(const DateTime&) const noexcept = default;

private:
    TimePoint value_;
};

struct DeadlineScale final {
    double elapsedShare = 0.0;
    std::int64_t remainingMinutes = 0;
    std::string label;
    enum class Urgency {
        normal,
        soon,
        urgent,
        overdue,
        completed
    } urgency = Urgency::normal;

    [[nodiscard]] static DeadlineScale calculate(
        const DateTime& periodStart,
        const DateTime& due,
        const DateTime& now,
        bool completed
    );
};

}  // namespace deadline
