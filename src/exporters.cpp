#include "deadline/exporters.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace deadline {
namespace {

std::string projectName(
    const ApplicationState& state,
    const std::string& identifier
) {
    const auto found = state.projects.find(identifier);
    return found == state.projects.end() ? "—" : found->second.title();
}

std::string csvEscape(const std::string& value) {
    const auto needsQuotes = value.find_first_of(",\"\r\n")
        != std::string::npos;
    if (!needsQuotes) {
        return value;
    }
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (const auto character : value) {
        if (character == '"') {
            escaped.push_back('"');
        }
        escaped.push_back(character);
    }
    escaped.push_back('"');
    return escaped;
}

std::string markdownEscape(std::string value) {
    std::size_t position = 0;
    while ((position = value.find('|', position)) != std::string::npos) {
        value.replace(position, 1, "\\|");
        position += 2;
    }
    std::replace(value.begin(), value.end(), '\n', ' ');
    std::replace(value.begin(), value.end(), '\r', ' ');
    return value;
}

std::string taskState(const Task& task) {
    return task.done() ? "Выполнена" : "Активна";
}

std::vector<std::reference_wrapper<const Task>> orderedTasks(
    const ApplicationState& state
) {
    std::vector<std::reference_wrapper<const Task>> result;
    result.reserve(state.tasks.size());
    for (const auto& task : state.tasks) {
        result.emplace_back(task);
    }
    std::sort(
        result.begin(),
        result.end(),
        [](const auto left, const auto right) {
            if (left.get().deadline() == right.get().deadline()) {
                return left.get().title() < right.get().title();
            }
            return left.get().deadline() < right.get().deadline();
        }
    );
    return result;
}

}  // namespace

ExportDocument::ExportDocument(
    std::string suggestedFileName,
    std::string mediaType,
    std::string content
)
    : suggestedFileName_(trim(std::move(suggestedFileName))),
      mediaType_(trim(std::move(mediaType))),
      content_(std::move(content)) {
    require(!suggestedFileName_.empty(), "Имя файла экспорта не задано");
    require(!mediaType_.empty(), "Тип данных экспорта не задан");
}

const std::string& ExportDocument::suggestedFileName() const noexcept {
    return suggestedFileName_;
}

const std::string& ExportDocument::mediaType() const noexcept {
    return mediaType_;
}

const std::string& ExportDocument::content() const noexcept {
    return content_;
}

std::string CsvTaskExporter::name() const {
    return "CSV";
}

ExportDocument CsvTaskExporter::exportState(
    const ApplicationState& state,
    const DateTime&
) const {
    std::ostringstream output;
    output << "id,title,project,type,deadline,priority,estimate_minutes,state"
           << "\r\n";
    for (const auto reference : orderedTasks(state)) {
        const auto& task = reference.get();
        output << csvEscape(task.id()) << ','
               << csvEscape(task.title()) << ','
               << csvEscape(projectName(state, task.projectId())) << ','
               << csvEscape(toString(task.type())) << ','
               << csvEscape(task.deadline().format()) << ','
               << task.priority() << ','
               << task.estimate().minutes() << ','
               << csvEscape(taskState(task)) << "\r\n";
    }
    return ExportDocument(
        "deadline_tasks.csv",
        "text/csv; charset=utf-8",
        output.str()
    );
}

std::string MarkdownTaskExporter::name() const {
    return "Markdown";
}

ExportDocument MarkdownTaskExporter::exportState(
    const ApplicationState& state,
    const DateTime& generatedAt
) const {
    std::ostringstream output;
    output << "# Deadline Planner — список задач\n\n"
           << "Сформировано: " << generatedAt.format() << "\n\n"
           << "| Статус | Задача | Проект | Тип | Срок | Приоритет |\n"
           << "|---|---|---|---|---|---:|\n";
    for (const auto reference : orderedTasks(state)) {
        const auto& task = reference.get();
        output << "| " << (task.done() ? "✓" : "○")
               << " | " << markdownEscape(task.title())
               << " | " << markdownEscape(projectName(state, task.projectId()))
               << " | " << toString(task.type())
               << " | " << task.deadline().format()
               << " | " << task.priority() << " |\n";
    }
    output << "\nВсего задач: " << state.tasks.size() << ".\n";
    return ExportDocument(
        "deadline_tasks.md",
        "text/markdown; charset=utf-8",
        output.str()
    );
}

ExportService::ExportService(const ApplicationState& state) noexcept
    : state_(state) {}

ExportDocument ExportService::run(const StateExporter& exporter) const {
    return exporter.exportState(state_);
}

std::vector<ExportDocument> ExportService::runAll(
    const std::vector<std::reference_wrapper<const StateExporter>>& exporters
) const {
    std::vector<ExportDocument> result;
    result.reserve(exporters.size());
    for (const auto exporter : exporters) {
        result.push_back(run(exporter.get()));
    }
    return result;
}

}  // namespace deadline
