#include "deadline/task.hpp"

#include <algorithm>
#include <utility>

namespace deadline {

std::string toString(const TaskType type) {
    switch (type) {
        case TaskType::deadline:
            return "deadline";
        case TaskType::recurring:
            return "recurring";
        case TaskType::checklist:
            return "checklist";
    }
    throw PlannerError("Неизвестный тип задачи");
}

TaskType taskTypeFromString(const std::string& value) {
    if (value == "deadline") {
        return TaskType::deadline;
    }
    if (value == "recurring") {
        return TaskType::recurring;
    }
    if (value == "checklist") {
        return TaskType::checklist;
    }
    throw PlannerError("Неизвестный тип задачи: " + value);
}

ChecklistItem::ChecklistItem()
    : ChecklistItem("Новый пункт") {}

ChecklistItem::ChecklistItem(std::string text)
    : id_(IdGenerator::create()),
      text_(validateText(text)) {}

ChecklistItem::ChecklistItem(
    std::string identifier,
    std::string text,
    const bool done
)
    : id_(std::move(identifier)),
      text_(validateText(text)),
      done_(done) {
    require(IdGenerator::isValid(id_), "Некорректный идентификатор пункта");
}

const std::string& ChecklistItem::id() const noexcept {
    return id_;
}

const std::string& ChecklistItem::text() const noexcept {
    return text_;
}

bool ChecklistItem::done() const noexcept {
    return done_;
}

void ChecklistItem::setDone(const bool done) noexcept {
    done_ = done;
}

std::string ChecklistItem::validateText(const std::string& text) {
    auto value = trim(text);
    require(!value.empty(), "Пункт чек-листа не должен быть пустым");
    require(value.size() <= 150, "Пункт чек-листа слишком длинный");
    return value;
}

Task::Task(TaskData data)
    : id_(std::move(data.id)),
      title_(validateTitle(data.title)),
      projectId_(std::move(data.projectId)),
      deadline_(data.deadline),
      createdAt_(data.createdAt),
      periodStart_(data.periodStart),
      priority_(data.priority),
      estimate_(data.estimate),
      description_(validateDescription(data.description)),
      completedAt_(data.completedAt) {
    require(IdGenerator::isValid(id_), "Некорректный идентификатор задачи");
    require(
        IdGenerator::isValid(projectId_),
        "Некорректный идентификатор проекта"
    );
    require(
        priority_ >= 1 && priority_ <= 3,
        "Приоритет должен быть от 1 до 3"
    );
    require(
        !(deadline_ < createdAt_),
        "Срок не может быть раньше создания задачи"
    );
    require(
        !(deadline_ < periodStart_),
        "Начало периода не может быть позже срока"
    );
}

const std::string& Task::id() const noexcept {
    return id_;
}

const std::string& Task::title() const noexcept {
    return title_;
}

const std::string& Task::projectId() const noexcept {
    return projectId_;
}

const DateTime& Task::deadline() const noexcept {
    return deadline_;
}

const DateTime& Task::createdAt() const noexcept {
    return createdAt_;
}

const DateTime& Task::periodStart() const noexcept {
    return periodStart_;
}

int Task::priority() const noexcept {
    return priority_;
}

TimeInterval Task::estimate() const noexcept {
    return estimate_;
}

const std::string& Task::description() const noexcept {
    return description_;
}

const std::optional<DateTime>& Task::completedAt() const noexcept {
    return completedAt_;
}

bool Task::done() const noexcept {
    return completedAt_.has_value();
}

bool Task::overdue(const DateTime& now) const noexcept {
    return !done() && deadline_ < now;
}

DeadlineScale Task::scale(const DateTime& now) const {
    return DeadlineScale::calculate(
        periodStart_,
        deadline_,
        now,
        done()
    );
}

void Task::rename(const std::string& title) {
    title_ = validateTitle(title);
}

void Task::reschedule(
    const DateTime& deadline,
    const DateTime& now
) {
    require(!done(), "Выполненную задачу нельзя перенести");
    require(now < deadline, "Новый срок должен быть в будущем");
    deadline_ = deadline;
    periodStart_ = now;
}

bool Task::equals(const Task& other) const noexcept {
    return type() == other.type()
        && id_ == other.id_
        && title_ == other.title_
        && projectId_ == other.projectId_
        && deadline_ == other.deadline_
        && createdAt_ == other.createdAt_
        && periodStart_ == other.periodStart_
        && priority_ == other.priority_
        && estimate_ == other.estimate_
        && description_ == other.description_
        && completedAt_ == other.completedAt_
        && sameDetails(other);
}

bool Task::operator==(const Task& other) const noexcept {
    return equals(other);
}

void Task::markCompleted(const DateTime& now) {
    require(!done(), "Задача уже выполнена");
    completedAt_ = now;
}

void Task::setDeadline(const DateTime& value) noexcept {
    deadline_ = value;
}

void Task::setPeriodStart(const DateTime& value) noexcept {
    periodStart_ = value;
}

void Task::clearCompletion() noexcept {
    completedAt_.reset();
}

std::string Task::validateTitle(const std::string& title) {
    auto value = trim(title);
    require(!value.empty(), "Название задачи не должно быть пустым");
    require(value.size() <= 150, "Название задачи слишком длинное");
    return value;
}

std::string Task::validateDescription(const std::string& description) {
    require(description.size() <= 3000, "Описание слишком длинное");
    return description;
}

DeadlineTask::DeadlineTask()
    : Task(TaskData{}) {}

DeadlineTask::DeadlineTask(TaskData data)
    : Task(std::move(data)) {}

DeadlineTask::DeadlineTask(
    std::string title,
    const DateTime deadline,
    std::string projectId
)
    : Task(TaskData{
        .title = std::move(title),
        .projectId = std::move(projectId),
        .deadline = deadline,
        .description = {},
        .completedAt = {}
    }) {}

TaskType DeadlineTask::type() const noexcept {
    return TaskType::deadline;
}

void DeadlineTask::complete(const DateTime& now) {
    markCompleted(now);
}

std::unique_ptr<Task> DeadlineTask::clone() const {
    return std::make_unique<DeadlineTask>(*this);
}

bool DeadlineTask::sameDetails(const Task&) const noexcept {
    return true;
}

RecurringTask::RecurringTask()
    : Task(TaskData{}),
      intervalDays_(7) {}

RecurringTask::RecurringTask(
    TaskData data,
    const int intervalDays,
    const int completionCount
)
    : Task(std::move(data)),
      intervalDays_(validateInterval(intervalDays)),
      completionCount_(completionCount) {
    require(
        completionCount_ >= 0,
        "Счётчик выполнений не может быть отрицательным"
    );
}

RecurringTask::RecurringTask(
    std::string title,
    const DateTime deadline,
    std::string projectId,
    const int intervalDays
)
    : Task(TaskData{
        .title = std::move(title),
        .projectId = std::move(projectId),
        .deadline = deadline,
        .description = {},
        .completedAt = {}
    }),
      intervalDays_(validateInterval(intervalDays)) {}

int RecurringTask::intervalDays() const noexcept {
    return intervalDays_;
}

int RecurringTask::completionCount() const noexcept {
    return completionCount_;
}

TaskType RecurringTask::type() const noexcept {
    return TaskType::recurring;
}

void RecurringTask::complete(const DateTime& now) {
    auto next = deadline().addDays(intervalDays_);
    while (!(now < next)) {
        next = next.addDays(intervalDays_);
    }
    ++completionCount_;
    setDeadline(next);
    setPeriodStart(now);
    clearCompletion();
}

std::unique_ptr<Task> RecurringTask::clone() const {
    return std::make_unique<RecurringTask>(*this);
}

bool RecurringTask::sameDetails(const Task& other) const noexcept {
    const auto& value = static_cast<const RecurringTask&>(other);
    return intervalDays_ == value.intervalDays_
        && completionCount_ == value.completionCount_;
}

int RecurringTask::validateInterval(const int value) {
    require(value >= 1 && value <= 365, "Период должен быть от 1 до 365 дней");
    return value;
}

ChecklistTask::ChecklistTask()
    : Task(TaskData{}),
      items_{ChecklistItem("Новый пункт")} {}

ChecklistTask::ChecklistTask(
    TaskData data,
    std::vector<ChecklistItem> items
)
    : Task(std::move(data)),
      items_(validateItems(std::move(items))) {}

ChecklistTask::ChecklistTask(
    std::string title,
    const DateTime deadline,
    std::string projectId,
    std::vector<ChecklistItem> items
)
    : Task(TaskData{
        .title = std::move(title),
        .projectId = std::move(projectId),
        .deadline = deadline,
        .description = {},
        .completedAt = {}
    }),
      items_(validateItems(std::move(items))) {}

const std::vector<ChecklistItem>& ChecklistTask::items() const noexcept {
    return items_;
}

std::size_t ChecklistTask::completedItemCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        items_.begin(),
        items_.end(),
        [](const ChecklistItem& item) {
            return item.done();
        }
    ));
}

bool ChecklistTask::allItemsDone() const noexcept {
    return completedItemCount() == items_.size();
}

void ChecklistTask::setItemDone(
    const std::string& itemId,
    const bool done
) {
    const auto item = std::find_if(
        items_.begin(),
        items_.end(),
        [&itemId](const ChecklistItem& candidate) {
            return candidate.id() == itemId;
        }
    );
    require(item != items_.end(), "Пункт чек-листа не найден");
    item->setDone(done);
}

TaskType ChecklistTask::type() const noexcept {
    return TaskType::checklist;
}

void ChecklistTask::complete(const DateTime& now) {
    require(allItemsDone(), "Сначала выполните все пункты чек-листа");
    markCompleted(now);
}

std::unique_ptr<Task> ChecklistTask::clone() const {
    return std::make_unique<ChecklistTask>(*this);
}

bool ChecklistTask::sameDetails(const Task& other) const noexcept {
    const auto& value = static_cast<const ChecklistTask&>(other);
    return items_ == value.items_;
}

std::vector<ChecklistItem> ChecklistTask::validateItems(
    std::vector<ChecklistItem> items
) {
    require(!items.empty(), "Чек-лист должен содержать хотя бы один пункт");
    return items;
}

}  // namespace deadline
