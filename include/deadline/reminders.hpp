#pragma once

#include <memory>
#include <string>
#include <vector>

#include "deadline/repository.hpp"

namespace deadline {

enum class ReminderLevel {
    information,
    warning,
    critical
};

[[nodiscard]] std::string toString(ReminderLevel level);

class Reminder final {
public:
    Reminder(
        std::string taskId,
        std::string ruleName,
        std::string message,
        ReminderLevel level
    );
    Reminder(const Reminder&) = default;
    Reminder(Reminder&&) noexcept = default;
    Reminder& operator=(const Reminder&) = default;
    Reminder& operator=(Reminder&&) noexcept = default;
    ~Reminder() = default;

    [[nodiscard]] const std::string& taskId() const noexcept;
    [[nodiscard]] const std::string& ruleName() const noexcept;
    [[nodiscard]] const std::string& message() const noexcept;
    [[nodiscard]] ReminderLevel level() const noexcept;

    bool operator==(const Reminder&) const noexcept = default;

private:
    std::string taskId_;
    std::string ruleName_;
    std::string message_;
    ReminderLevel level_ = ReminderLevel::information;
};

class ReminderRule {
public:
    virtual ~ReminderRule() = default;
    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual bool matches(
        const Task& task,
        const DateTime& now
    ) const noexcept = 0;
    [[nodiscard]] virtual Reminder make(
        const Task& task,
        const DateTime& now
    ) const = 0;
    [[nodiscard]] virtual std::unique_ptr<ReminderRule> clone() const = 0;
};

class OverdueReminderRule final : public ReminderRule {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] bool matches(
        const Task& task,
        const DateTime& now
    ) const noexcept override;
    [[nodiscard]] Reminder make(
        const Task& task,
        const DateTime& now
    ) const override;
    [[nodiscard]] std::unique_ptr<ReminderRule> clone() const override;
};

class UrgentReminderRule final : public ReminderRule {
public:
    explicit UrgentReminderRule(int hoursBefore = 24);

    [[nodiscard]] int hoursBefore() const noexcept;
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] bool matches(
        const Task& task,
        const DateTime& now
    ) const noexcept override;
    [[nodiscard]] Reminder make(
        const Task& task,
        const DateTime& now
    ) const override;
    [[nodiscard]] std::unique_ptr<ReminderRule> clone() const override;

private:
    int hoursBefore_ = 24;
};

class HighPriorityReminderRule final : public ReminderRule {
public:
    explicit HighPriorityReminderRule(int daysBefore = 3);

    [[nodiscard]] int daysBefore() const noexcept;
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] bool matches(
        const Task& task,
        const DateTime& now
    ) const noexcept override;
    [[nodiscard]] Reminder make(
        const Task& task,
        const DateTime& now
    ) const override;
    [[nodiscard]] std::unique_ptr<ReminderRule> clone() const override;

private:
    int daysBefore_ = 3;
};

class ReminderEngine final {
public:
    ReminderEngine();
    explicit ReminderEngine(
        std::vector<std::unique_ptr<ReminderRule>> rules
    );
    ReminderEngine(const ReminderEngine& other);
    ReminderEngine(ReminderEngine&&) noexcept = default;
    ReminderEngine& operator=(ReminderEngine other) noexcept;
    ~ReminderEngine() = default;

    void addRule(std::unique_ptr<ReminderRule> rule);
    [[nodiscard]] std::size_t ruleCount() const noexcept;
    [[nodiscard]] std::vector<Reminder> evaluate(
        const ApplicationState& state,
        const DateTime& now = DateTime::now()
    ) const;
    void swap(ReminderEngine& other) noexcept;

private:
    std::vector<std::unique_ptr<ReminderRule>> rules_;
};

void swap(ReminderEngine& left, ReminderEngine& right) noexcept;

}  // namespace deadline
