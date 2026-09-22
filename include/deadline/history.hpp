#pragma once

#include <string>

#include "deadline/common.hpp"
#include "deadline/date_time.hpp"

namespace deadline {

enum class HistoryAction {
    created,
    completed,
    repeated,
    rescheduled,
    checklistChanged,
    removed
};

[[nodiscard]] std::string toString(HistoryAction action);
[[nodiscard]] HistoryAction historyActionFromString(
    const std::string& value
);

class HistoryEntry final {
public:
    HistoryEntry();
    HistoryEntry(
        std::string taskId,
        HistoryAction action,
        DateTime happenedAt,
        std::string details = {}
    );
    HistoryEntry(
        std::string identifier,
        std::string taskId,
        HistoryAction action,
        DateTime happenedAt,
        std::string details
    );
    HistoryEntry(const HistoryEntry&) = default;
    HistoryEntry(HistoryEntry&&) noexcept = default;
    HistoryEntry& operator=(const HistoryEntry&) = default;
    HistoryEntry& operator=(HistoryEntry&&) noexcept = default;
    ~HistoryEntry() = default;

    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& taskId() const noexcept;
    [[nodiscard]] HistoryAction action() const noexcept;
    [[nodiscard]] const DateTime& happenedAt() const noexcept;
    [[nodiscard]] const std::string& details() const noexcept;

    bool operator==(const HistoryEntry&) const noexcept = default;

private:
    std::string id_;
    std::string taskId_;
    HistoryAction action_ = HistoryAction::created;
    DateTime happenedAt_;
    std::string details_;
};

}  // namespace deadline
