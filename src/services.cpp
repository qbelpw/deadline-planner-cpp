#include "deadline/services.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace deadline {
namespace {

std::string lower(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        }
    );
    return value;
}

std::unique_ptr<Task> makeTask(const TaskDraft& draft, const DateTime& now) {
    TaskData data;
    data.title = draft.title;
    data.projectId = draft.projectId;
    data.deadline = draft.deadline;
    data.createdAt = now;
    data.periodStart = now;
    data.priority = draft.priority;
    data.estimate = draft.estimate;
    data.description = draft.description;

    switch (draft.type) {
        case TaskType::deadline:
            return std::make_unique<DeadlineTask>(std::move(data));
        case TaskType::recurring:
            return std::make_unique<RecurringTask>(
                std::move(data),
                draft.intervalDays
            );
        case TaskType::checklist: {
            std::vector<ChecklistItem> items;
            items.reserve(draft.checklistItems.size());
            for (const auto& text : draft.checklistItems) {
                items.emplace_back(text);
            }
            return std::make_unique<ChecklistTask>(
                std::move(data),
                std::move(items)
            );
        }
    }
    throw PlannerError("Неизвестный тип задачи");
}

}  // namespace

TaskService::TaskService(
    TaskRepository& repository,
    DateTimeProvider clock
)
    : repository_(repository),
      clock_(std::move(clock)),
      state_(repository_.load()) {
    require(static_cast<bool>(clock_), "Функция времени не задана");
}

const ApplicationState& TaskService::state() const noexcept {
    return state_;
}

const EntityHashTable<Task>& TaskService::tasks() const noexcept {
    return state_.tasks;
}

std::string TaskService::addTask(const TaskDraft& draft) {
    return addTask(makeTask(draft, now()));
}

std::string TaskService::addTask(std::unique_ptr<Task> task) {
    require(task != nullptr, "Задача не задана");
    requireProjectAvailable(task->projectId());
    const auto identifier = task->id();
    auto next = state_;
    next.tasks.add(std::move(task));
    next.history.emplace_back(
        identifier,
        HistoryAction::created,
        now(),
        "Задача создана"
    );
    commit(std::move(next));
    return identifier;
}

std::string TaskService::addTask(
    const std::string& title,
    const DateTime& deadline,
    const std::string& projectId
) {
    return addTask(
        title,
        deadline,
        projectId,
        2,
        TimeInterval(60),
        ""
    );
}

std::string TaskService::addTask(
    const std::string& title,
    const DateTime& deadline,
    const std::string& projectId,
    const int priority,
    const TimeInterval estimate,
    const std::string& description
) {
    TaskDraft draft;
    draft.title = title;
    draft.deadline = deadline;
    draft.projectId = projectId;
    draft.priority = priority;
    draft.estimate = estimate;
    draft.description = description;
    return addTask(draft);
}

void TaskService::complete(const std::string& identifier) {
    auto next = state_;
    auto& task = next.tasks[identifier];
    const auto wasRecurring = task.type() == TaskType::recurring;
    const auto eventTime = now();
    task.complete(eventTime);
    next.history.emplace_back(
        identifier,
        wasRecurring ? HistoryAction::repeated : HistoryAction::completed,
        eventTime,
        wasRecurring ? "Повторение выполнено" : "Задача выполнена"
    );
    commit(std::move(next));
}

void TaskService::setChecklistItem(
    const std::string& taskId,
    const std::string& itemId,
    const bool done
) {
    auto next = state_;
    auto& task = next.tasks[taskId];
    require(
        task.type() == TaskType::checklist,
        "Выбранная задача не является чек-листом"
    );
    auto& checklist = static_cast<ChecklistTask&>(task);
    checklist.setItemDone(itemId, done);
    next.history.emplace_back(
        taskId,
        HistoryAction::checklistChanged,
        now(),
        done ? "Пункт выполнен" : "Пункт снова активен"
    );
    commit(std::move(next));
}

void TaskService::reschedule(
    const std::string& identifier,
    const DateTime& deadline
) {
    auto next = state_;
    next.tasks[identifier].reschedule(deadline, now());
    next.history.emplace_back(
        identifier,
        HistoryAction::rescheduled,
        now(),
        deadline.format()
    );
    commit(std::move(next));
}

void TaskService::remove(const std::string& identifier) {
    auto next = state_;
    require(next.tasks.remove(identifier), "Задача не найдена");
    next.history.emplace_back(
        identifier,
        HistoryAction::removed,
        now(),
        "Задача удалена"
    );
    commit(std::move(next));
}

const Task& TaskService::find(const std::string& identifier) const {
    return state_.tasks[identifier];
}

std::vector<std::reference_wrapper<const Task>> TaskService::find(
    const TaskFilter& filter
) const {
    std::vector<std::reference_wrapper<const Task>> result;
    const auto search = lower(trim(filter.search));
    for (const auto& task : state_.tasks) {
        if (filter.projectId.has_value()
            && task.projectId() != *filter.projectId) {
            continue;
        }
        if (filter.completed.has_value()
            && task.done() != *filter.completed) {
            continue;
        }
        if (filter.dueBefore.has_value()
            && *filter.dueBefore < task.deadline()) {
            continue;
        }
        if (!search.empty()) {
            const auto haystack = lower(
                task.title() + " " + task.description()
            );
            if (haystack.find(search) == std::string::npos) {
                continue;
            }
        }
        result.emplace_back(task);
    }
    std::sort(
        result.begin(),
        result.end(),
        [](const auto left, const auto right) {
            const auto& first = left.get();
            const auto& second = right.get();
            if (first.deadline() == second.deadline()) {
                return first.priority() > second.priority();
            }
            return first.deadline() < second.deadline();
        }
    );
    return result;
}

void TaskService::replaceState(ApplicationState state) {
    commit(std::move(state));
}

void TaskService::commit(ApplicationState state) {
    repository_.save(state);
    state_ = std::move(state);
}

void TaskService::requireProjectAvailable(
    const std::string& projectId
) const {
    const auto project = state_.projects.find(projectId);
    require(project != state_.projects.end(), "Проект не найден");
    require(!project->second.archived(), "Проект находится в архиве");
}

DateTime TaskService::now() const {
    return clock_();
}

ProjectService::ProjectService(TaskService& tasks) noexcept
    : tasks_(tasks) {}

std::string ProjectService::add(const std::string& title) {
    auto state = tasks_.state();
    const auto duplicate = std::find_if(
        state.projects.begin(),
        state.projects.end(),
        [&title](const auto& value) {
            return value.second.title() == trim(title);
        }
    );
    require(duplicate == state.projects.end(), "Такой проект уже существует");
    Project project(title);
    const auto identifier = project.id();
    state.projects.emplace(identifier, std::move(project));
    tasks_.replaceState(std::move(state));
    return identifier;
}

void ProjectService::rename(
    const std::string& identifier,
    const std::string& title
) {
    auto state = tasks_.state();
    const auto project = state.projects.find(identifier);
    require(project != state.projects.end(), "Проект не найден");
    project->second.rename(title);
    tasks_.replaceState(std::move(state));
}

void ProjectService::archive(
    const std::string& identifier,
    const bool archived
) {
    auto state = tasks_.state();
    const auto project = state.projects.find(identifier);
    require(project != state.projects.end(), "Проект не найден");
    if (archived) {
        for (const auto& task : state.tasks) {
            require(
                task.projectId() != identifier || task.done(),
                "Проект с активными задачами нельзя архивировать"
            );
        }
    }
    project->second.setArchived(archived);
    tasks_.replaceState(std::move(state));
}

}  // namespace deadline
