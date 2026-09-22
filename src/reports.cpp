#include "deadline/reports.hpp"

#include <algorithm>
#include <map>
#include <sstream>

namespace deadline {
namespace {

std::string projectTitle(
    const ApplicationState& state,
    const std::string& identifier
) {
    const auto project = state.projects.find(identifier);
    return project == state.projects.end()
        ? std::string("Неизвестный проект")
        : project->second.title();
}

std::string monthKey(const DateTime& value) {
    return value.format().substr(0, 7);
}

}  // namespace

std::string OverdueReport::title() const {
    return "Просроченные задачи";
}

ReportResult<ReportRow> OverdueReport::generate(
    const ApplicationState& state,
    const DateTime& now
) const {
    std::vector<ReportRow> rows;
    for (const auto& task : state.tasks) {
        if (!task.overdue(now)) {
            continue;
        }
        const auto overdueMinutes = task.deadline().minutesUntil(now);
        rows.push_back({
            task.title(),
            projectTitle(state, task.projectId()),
            task.deadline().format(),
            std::to_string(overdueMinutes / (24 * 60))
        });
    }
    std::sort(rows.begin(), rows.end());
    return ReportResult<ReportRow>(
        {"Задача", "Проект", "Срок", "Дней просрочки"},
        std::move(rows)
    );
}

std::string WorkloadReport::title() const {
    return "Нагрузка по датам";
}

ReportResult<ReportRow> WorkloadReport::generate(
    const ApplicationState& state,
    const DateTime&
) const {
    std::map<std::string, std::pair<int, std::int64_t>> totals;
    for (const auto& task : state.tasks) {
        if (task.done()) {
            continue;
        }
        const auto day = task.deadline().format().substr(0, 10);
        auto& [count, minutes] = totals[day];
        ++count;
        minutes += task.estimate().minutes();
    }
    std::vector<ReportRow> rows;
    for (const auto& [day, values] : totals) {
        rows.push_back({
            day,
            std::to_string(values.first),
            std::to_string(values.second)
        });
    }
    return ReportResult<ReportRow>(
        {"Дата", "Активных задач", "Минут работы"},
        std::move(rows)
    );
}

std::string CompletionReport::title() const {
    return "Выполнения по месяцам";
}

ReportResult<ReportRow> CompletionReport::generate(
    const ApplicationState& state,
    const DateTime&
) const {
    std::map<std::string, int> totals;
    for (const auto& entry : state.history) {
        if (entry.action() == HistoryAction::completed
            || entry.action() == HistoryAction::repeated) {
            ++totals[monthKey(entry.happenedAt())];
        }
    }
    std::vector<ReportRow> rows;
    for (const auto& [month, count] : totals) {
        rows.push_back({month, std::to_string(count)});
    }
    return ReportResult<ReportRow>(
        {"Месяц", "Выполнений"},
        std::move(rows)
    );
}

ReportService::ReportService(const ApplicationState& state) noexcept
    : state_(state) {}

ReportResult<ReportRow> ReportService::generate(
    const Report& report,
    const DateTime& now
) const {
    return report.generate(state_, now);
}

}  // namespace deadline
