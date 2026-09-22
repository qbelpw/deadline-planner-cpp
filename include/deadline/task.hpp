#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "deadline/common.hpp"
#include "deadline/date_time.hpp"

namespace deadline {

enum class TaskType {
    deadline,
    recurring,
    checklist
};

[[nodiscard]] std::string toString(TaskType type);
[[nodiscard]] TaskType taskTypeFromString(const std::string& value);

struct TaskData final {
    std::string id = IdGenerator::create();
    std::string title = "Новая задача";
    std::string projectId = IdGenerator::create();
    DateTime deadline = DateTime::now().addDays(7);
    DateTime createdAt = DateTime::now();
    DateTime periodStart = createdAt;
    int priority = 2;
    TimeInterval estimate = TimeInterval(60);
    std::string description;
    std::optional<DateTime> completedAt;
};

class ChecklistItem final {
public:
    ChecklistItem();
    explicit ChecklistItem(std::string text);
    ChecklistItem(std::string identifier, std::string text, bool done);
    ChecklistItem(const ChecklistItem&) = default;
    ChecklistItem(ChecklistItem&&) noexcept = default;
    ChecklistItem& operator=(const ChecklistItem&) = default;
    ChecklistItem& operator=(ChecklistItem&&) noexcept = default;
    ~ChecklistItem() = default;

    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& text() const noexcept;
    [[nodiscard]] bool done() const noexcept;
    void setDone(bool done) noexcept;

    bool operator==(const ChecklistItem&) const noexcept = default;

private:
    static std::string validateText(const std::string& text);

    std::string id_;
    std::string text_;
    bool done_ = false;
};

class Task {
public:
    virtual ~Task() = default;

    Task(const Task&) = default;
    Task(Task&&) noexcept = default;
    Task& operator=(const Task&) = default;
    Task& operator=(Task&&) noexcept = default;

    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& title() const noexcept;
    [[nodiscard]] const std::string& projectId() const noexcept;
    [[nodiscard]] const DateTime& deadline() const noexcept;
    [[nodiscard]] const DateTime& createdAt() const noexcept;
    [[nodiscard]] const DateTime& periodStart() const noexcept;
    [[nodiscard]] int priority() const noexcept;
    [[nodiscard]] TimeInterval estimate() const noexcept;
    [[nodiscard]] const std::string& description() const noexcept;
    [[nodiscard]] const std::optional<DateTime>& completedAt() const noexcept;
    [[nodiscard]] bool done() const noexcept;
    [[nodiscard]] bool overdue(const DateTime& now) const noexcept;
    [[nodiscard]] DeadlineScale scale(const DateTime& now) const;

    void rename(const std::string& title);
    void reschedule(const DateTime& deadline, const DateTime& now);

    [[nodiscard]] virtual TaskType type() const noexcept = 0;
    virtual void complete(const DateTime& now) = 0;
    [[nodiscard]] virtual std::unique_ptr<Task> clone() const = 0;

    [[nodiscard]] bool equals(const Task& other) const noexcept;
    bool operator==(const Task& other) const noexcept;

protected:
    explicit Task(TaskData data);

    void markCompleted(const DateTime& now);
    void setDeadline(const DateTime& value) noexcept;
    void setPeriodStart(const DateTime& value) noexcept;
    void clearCompletion() noexcept;

    [[nodiscard]] virtual bool sameDetails(
        const Task& other
    ) const noexcept = 0;

private:
    static std::string validateTitle(const std::string& title);
    static std::string validateDescription(const std::string& value);

    std::string id_;
    std::string title_;
    std::string projectId_;
    DateTime deadline_;
    DateTime createdAt_;
    DateTime periodStart_;
    int priority_ = 2;
    TimeInterval estimate_;
    std::string description_;
    std::optional<DateTime> completedAt_;
};

class DeadlineTask final : public Task {
public:
    DeadlineTask();
    explicit DeadlineTask(TaskData data);
    DeadlineTask(
        std::string title,
        DateTime deadline,
        std::string projectId
    );
    DeadlineTask(const DeadlineTask&) = default;
    DeadlineTask(DeadlineTask&&) noexcept = default;
    DeadlineTask& operator=(const DeadlineTask&) = default;
    DeadlineTask& operator=(DeadlineTask&&) noexcept = default;
    ~DeadlineTask() override = default;

    [[nodiscard]] TaskType type() const noexcept override;
    void complete(const DateTime& now) override;
    [[nodiscard]] std::unique_ptr<Task> clone() const override;

protected:
    [[nodiscard]] bool sameDetails(
        const Task& other
    ) const noexcept override;
};

class RecurringTask final : public Task {
public:
    RecurringTask();
    RecurringTask(TaskData data, int intervalDays, int completionCount = 0);
    RecurringTask(
        std::string title,
        DateTime deadline,
        std::string projectId,
        int intervalDays
    );
    RecurringTask(const RecurringTask&) = default;
    RecurringTask(RecurringTask&&) noexcept = default;
    RecurringTask& operator=(const RecurringTask&) = default;
    RecurringTask& operator=(RecurringTask&&) noexcept = default;
    ~RecurringTask() override = default;

    [[nodiscard]] int intervalDays() const noexcept;
    [[nodiscard]] int completionCount() const noexcept;

    [[nodiscard]] TaskType type() const noexcept override;
    void complete(const DateTime& now) override;
    [[nodiscard]] std::unique_ptr<Task> clone() const override;

protected:
    [[nodiscard]] bool sameDetails(
        const Task& other
    ) const noexcept override;

private:
    static int validateInterval(int value);

    int intervalDays_ = 7;
    int completionCount_ = 0;
};

class ChecklistTask final : public Task {
public:
    ChecklistTask();
    ChecklistTask(TaskData data, std::vector<ChecklistItem> items);
    ChecklistTask(
        std::string title,
        DateTime deadline,
        std::string projectId,
        std::vector<ChecklistItem> items
    );
    ChecklistTask(const ChecklistTask&) = default;
    ChecklistTask(ChecklistTask&&) noexcept = default;
    ChecklistTask& operator=(const ChecklistTask&) = default;
    ChecklistTask& operator=(ChecklistTask&&) noexcept = default;
    ~ChecklistTask() override = default;

    [[nodiscard]] const std::vector<ChecklistItem>& items() const noexcept;
    [[nodiscard]] std::size_t completedItemCount() const noexcept;
    [[nodiscard]] bool allItemsDone() const noexcept;
    void setItemDone(const std::string& itemId, bool done);

    [[nodiscard]] TaskType type() const noexcept override;
    void complete(const DateTime& now) override;
    [[nodiscard]] std::unique_ptr<Task> clone() const override;

protected:
    [[nodiscard]] bool sameDetails(
        const Task& other
    ) const noexcept override;

private:
    static std::vector<ChecklistItem> validateItems(
        std::vector<ChecklistItem> items
    );

    std::vector<ChecklistItem> items_;
};

}  // namespace deadline
