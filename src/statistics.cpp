#include "deadline/statistics.hpp"

#include <algorithm>
#include <unordered_map>

namespace deadline {

double ProjectStatistics::completionRate() const noexcept {
    const auto count = active + completed;
    return count == 0
        ? 0.0
        : static_cast<double>(completed) / static_cast<double>(count);
}

std::size_t DashboardStatistics::total() const noexcept {
    return active + completed;
}

double DashboardStatistics::completionRate() const noexcept {
    return total() == 0
        ? 0.0
        : static_cast<double>(completed) / static_cast<double>(total());
}

StatisticsService::StatisticsService(
    const ApplicationState& state
) noexcept
    : state_(state) {}

DashboardStatistics StatisticsService::calculate(
    const DateTime& now
) const {
    DashboardStatistics result;
    std::unordered_map<std::string, std::size_t> indexes;
    result.projects.reserve(state_.projects.size());
    for (const auto& [identifier, project] : state_.projects) {
        indexes.emplace(identifier, result.projects.size());
        result.projects.push_back(ProjectStatistics{
            identifier,
            project.title()
        });
    }

    for (const auto& task : state_.tasks) {
        const auto projectIndex = indexes.find(task.projectId());
        if (projectIndex == indexes.end()) {
            continue;
        }
        auto& project = result.projects[projectIndex->second];
        if (task.done()) {
            ++result.completed;
            ++project.completed;
            continue;
        }
        ++result.active;
        ++project.active;
        result.plannedMinutes += task.estimate().minutes();
        project.plannedMinutes += task.estimate().minutes();
        const auto remaining = now.minutesUntil(task.deadline());
        if (remaining < 0) {
            ++result.overdue;
            ++project.overdue;
        } else if (remaining <= 3 * 24 * 60) {
            ++result.dueSoon;
        }
    }
    std::sort(
        result.projects.begin(),
        result.projects.end(),
        [](const ProjectStatistics& left, const ProjectStatistics& right) {
            if (left.overdue != right.overdue) {
                return left.overdue > right.overdue;
            }
            return left.projectTitle < right.projectTitle;
        }
    );
    return result;
}

}  // namespace deadline
