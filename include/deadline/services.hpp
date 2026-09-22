#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "deadline/repository.hpp"

namespace deadline {

struct TaskDraft final {
    TaskType type = TaskType::deadline;
    std::string title;
    std::string projectId;
    DateTime deadline;
    int priority = 2;
    TimeInterval estimate = TimeInterval(60);
    std::string description;
    int intervalDays = 7;
    std::vector<std::string> checklistItems;
};

struct TaskFilter final {
    std::string search;
    std::optional<std::string> projectId;
    std::optional<bool> completed;
    std::optional<DateTime> dueBefore;
};

using DateTimeProvider = std::function<DateTime()>;

class TaskService final {
public:
    explicit TaskService(
        TaskRepository& repository,
        DateTimeProvider clock = DateTime::now
    );
    TaskService(const TaskService&) = delete;
    TaskService(TaskService&&) = delete;
    TaskService& operator=(const TaskService&) = delete;
    TaskService& operator=(TaskService&&) = delete;
    ~TaskService() = default;

    [[nodiscard]] const ApplicationState& state() const noexcept;
    [[nodiscard]] const EntityHashTable<Task>& tasks() const noexcept;

    std::string addTask(const TaskDraft& draft);
    std::string addTask(std::unique_ptr<Task> task);
    std::string addTask(
        const std::string& title,
        const DateTime& deadline,
        const std::string& projectId
    );
    std::string addTask(
        const std::string& title,
        const DateTime& deadline,
        const std::string& projectId,
        int priority,
        TimeInterval estimate,
        const std::string& description
    );

    void complete(const std::string& identifier);
    void setChecklistItem(
        const std::string& taskId,
        const std::string& itemId,
        bool done
    );
    void reschedule(
        const std::string& identifier,
        const DateTime& deadline
    );
    void remove(const std::string& identifier);

    [[nodiscard]] const Task& find(const std::string& identifier) const;
    [[nodiscard]] std::vector<std::reference_wrapper<const Task>> find(
        const TaskFilter& filter
    ) const;

    void replaceState(ApplicationState state);

private:
    void commit(ApplicationState state);
    void requireProjectAvailable(const std::string& projectId) const;
    [[nodiscard]] DateTime now() const;

    TaskRepository& repository_;
    DateTimeProvider clock_;
    ApplicationState state_;
};

class ProjectService final {
public:
    explicit ProjectService(TaskService& tasks) noexcept;

    std::string add(const std::string& title);
    void rename(
        const std::string& identifier,
        const std::string& title
    );
    void archive(const std::string& identifier, bool archived = true);

private:
    TaskService& tasks_;
};

}  // namespace deadline
