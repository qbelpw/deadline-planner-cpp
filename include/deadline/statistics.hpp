#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "deadline/repository.hpp"

namespace deadline {

struct ProjectStatistics final {
    std::string projectId;
    std::string projectTitle;
    std::size_t active = 0;
    std::size_t completed = 0;
    std::size_t overdue = 0;
    std::int64_t plannedMinutes = 0;

    [[nodiscard]] double completionRate() const noexcept;
    bool operator==(const ProjectStatistics&) const noexcept = default;
};

struct DashboardStatistics final {
    std::size_t active = 0;
    std::size_t dueSoon = 0;
    std::size_t overdue = 0;
    std::size_t completed = 0;
    std::int64_t plannedMinutes = 0;
    std::vector<ProjectStatistics> projects;

    [[nodiscard]] std::size_t total() const noexcept;
    [[nodiscard]] double completionRate() const noexcept;
    bool operator==(const DashboardStatistics&) const noexcept = default;
};

class StatisticsService final {
public:
    explicit StatisticsService(const ApplicationState& state) noexcept;

    [[nodiscard]] DashboardStatistics calculate(
        const DateTime& now = DateTime::now()
    ) const;

private:
    const ApplicationState& state_;
};

}  // namespace deadline
