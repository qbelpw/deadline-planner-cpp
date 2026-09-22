#include "deadline/history.hpp"

#include <utility>

namespace deadline {

std::string toString(const HistoryAction action) {
    switch (action) {
        case HistoryAction::created:
            return "created";
        case HistoryAction::completed:
            return "completed";
        case HistoryAction::repeated:
            return "repeated";
        case HistoryAction::rescheduled:
            return "rescheduled";
        case HistoryAction::checklistChanged:
            return "checklist_changed";
        case HistoryAction::removed:
            return "removed";
    }
    throw PlannerError("Неизвестное действие истории");
}

HistoryAction historyActionFromString(const std::string& value) {
    if (value == "created") {
        return HistoryAction::created;
    }
    if (value == "completed") {
        return HistoryAction::completed;
    }
    if (value == "repeated") {
        return HistoryAction::repeated;
    }
    if (value == "rescheduled") {
        return HistoryAction::rescheduled;
    }
    if (value == "checklist_changed") {
        return HistoryAction::checklistChanged;
    }
    if (value == "removed") {
        return HistoryAction::removed;
    }
    throw PlannerError("Неизвестное действие истории: " + value);
}

HistoryEntry::HistoryEntry()
    : HistoryEntry(
        IdGenerator::create(),
        HistoryAction::created,
        DateTime::now()
    ) {}

HistoryEntry::HistoryEntry(
    std::string taskId,
    const HistoryAction action,
    const DateTime happenedAt,
    std::string details
)
    : HistoryEntry(
        IdGenerator::create(),
        std::move(taskId),
        action,
        happenedAt,
        std::move(details)
    ) {}

HistoryEntry::HistoryEntry(
    std::string identifier,
    std::string taskId,
    const HistoryAction action,
    const DateTime happenedAt,
    std::string details
)
    : id_(std::move(identifier)),
      taskId_(std::move(taskId)),
      action_(action),
      happenedAt_(happenedAt),
      details_(std::move(details)) {
    require(IdGenerator::isValid(id_), "Некорректный идентификатор истории");
    require(
        IdGenerator::isValid(taskId_),
        "Некорректный идентификатор задачи в истории"
    );
    require(details_.size() <= 1000, "Описание события слишком длинное");
}

const std::string& HistoryEntry::id() const noexcept {
    return id_;
}

const std::string& HistoryEntry::taskId() const noexcept {
    return taskId_;
}

HistoryAction HistoryEntry::action() const noexcept {
    return action_;
}

const DateTime& HistoryEntry::happenedAt() const noexcept {
    return happenedAt_;
}

const std::string& HistoryEntry::details() const noexcept {
    return details_;
}

}  // namespace deadline
