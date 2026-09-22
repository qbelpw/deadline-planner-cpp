#include "deadline/reminders.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace deadline {
namespace {

std::string compactDuration(const std::int64_t minutes) {
    const auto value = std::max<std::int64_t>(0, minutes);
    const auto days = value / (24 * 60);
    const auto hours = value % (24 * 60) / 60;
    const auto rest = value % 60;
    std::ostringstream output;
    if (days > 0) {
        output << days << " д. ";
    }
    if (hours > 0 || days > 0) {
        output << hours << " ч. ";
    }
    output << rest << " мин.";
    return output.str();
}

int levelRank(const ReminderLevel level) noexcept {
    switch (level) {
        case ReminderLevel::information:
            return 0;
        case ReminderLevel::warning:
            return 1;
        case ReminderLevel::critical:
            return 2;
    }
    return 0;
}

}  // namespace

std::string toString(const ReminderLevel level) {
    switch (level) {
        case ReminderLevel::information:
            return "Информация";
        case ReminderLevel::warning:
            return "Предупреждение";
        case ReminderLevel::critical:
            return "Критично";
    }
    throw PlannerError("Неизвестный уровень напоминания");
}

Reminder::Reminder(
    std::string taskId,
    std::string ruleName,
    std::string message,
    const ReminderLevel level
)
    : taskId_(std::move(taskId)),
      ruleName_(trim(std::move(ruleName))),
      message_(trim(std::move(message))),
      level_(level) {
    require(IdGenerator::isValid(taskId_), "Некорректная задача напоминания");
    require(!ruleName_.empty(), "Название правила не задано");
    require(!message_.empty(), "Текст напоминания не задан");
}

const std::string& Reminder::taskId() const noexcept {
    return taskId_;
}

const std::string& Reminder::ruleName() const noexcept {
    return ruleName_;
}

const std::string& Reminder::message() const noexcept {
    return message_;
}

ReminderLevel Reminder::level() const noexcept {
    return level_;
}

std::string OverdueReminderRule::name() const {
    return "Контроль просрочки";
}

bool OverdueReminderRule::matches(
    const Task& task,
    const DateTime& now
) const noexcept {
    return task.overdue(now);
}

Reminder OverdueReminderRule::make(
    const Task& task,
    const DateTime& now
) const {
    require(matches(task, now), "Правило просрочки неприменимо");
    return Reminder(
        task.id(),
        name(),
        "«" + task.title() + "» просрочена на "
            + compactDuration(task.deadline().minutesUntil(now)),
        ReminderLevel::critical
    );
}

std::unique_ptr<ReminderRule> OverdueReminderRule::clone() const {
    return std::make_unique<OverdueReminderRule>(*this);
}

UrgentReminderRule::UrgentReminderRule(const int hoursBefore)
    : hoursBefore_(hoursBefore) {
    require(
        hoursBefore_ >= 1 && hoursBefore_ <= 24 * 30,
        "Срочный интервал должен быть от 1 до 720 часов"
    );
}

int UrgentReminderRule::hoursBefore() const noexcept {
    return hoursBefore_;
}

std::string UrgentReminderRule::name() const {
    return "Ближайший срок";
}

bool UrgentReminderRule::matches(
    const Task& task,
    const DateTime& now
) const noexcept {
    const auto remaining = now.minutesUntil(task.deadline());
    return !task.done()
        && remaining >= 0
        && remaining <= static_cast<std::int64_t>(hoursBefore_) * 60;
}

Reminder UrgentReminderRule::make(
    const Task& task,
    const DateTime& now
) const {
    require(matches(task, now), "Срочное правило неприменимо");
    return Reminder(
        task.id(),
        name(),
        "До «" + task.title() + "» осталось "
            + compactDuration(now.minutesUntil(task.deadline())),
        ReminderLevel::warning
    );
}

std::unique_ptr<ReminderRule> UrgentReminderRule::clone() const {
    return std::make_unique<UrgentReminderRule>(*this);
}

HighPriorityReminderRule::HighPriorityReminderRule(const int daysBefore)
    : daysBefore_(daysBefore) {
    require(
        daysBefore_ >= 1 && daysBefore_ <= 30,
        "Интервал приоритетного правила должен быть от 1 до 30 дней"
    );
}

int HighPriorityReminderRule::daysBefore() const noexcept {
    return daysBefore_;
}

std::string HighPriorityReminderRule::name() const {
    return "Высокий приоритет";
}

bool HighPriorityReminderRule::matches(
    const Task& task,
    const DateTime& now
) const noexcept {
    const auto remaining = now.minutesUntil(task.deadline());
    return !task.done()
        && task.priority() == 3
        && remaining > 24 * 60
        && remaining <= static_cast<std::int64_t>(daysBefore_) * 24 * 60;
}

Reminder HighPriorityReminderRule::make(
    const Task& task,
    const DateTime& now
) const {
    require(matches(task, now), "Приоритетное правило неприменимо");
    return Reminder(
        task.id(),
        name(),
        "Высокий приоритет: «" + task.title() + "», осталось "
            + compactDuration(now.minutesUntil(task.deadline())),
        ReminderLevel::information
    );
}

std::unique_ptr<ReminderRule> HighPriorityReminderRule::clone() const {
    return std::make_unique<HighPriorityReminderRule>(*this);
}

ReminderEngine::ReminderEngine() {
    rules_.push_back(std::make_unique<OverdueReminderRule>());
    rules_.push_back(std::make_unique<UrgentReminderRule>());
    rules_.push_back(std::make_unique<HighPriorityReminderRule>());
}

ReminderEngine::ReminderEngine(
    std::vector<std::unique_ptr<ReminderRule>> rules
) {
    for (auto& rule : rules) {
        addRule(std::move(rule));
    }
}

ReminderEngine::ReminderEngine(const ReminderEngine& other) {
    rules_.reserve(other.rules_.size());
    for (const auto& rule : other.rules_) {
        rules_.push_back(rule->clone());
    }
}

ReminderEngine& ReminderEngine::operator=(ReminderEngine other) noexcept {
    swap(other);
    return *this;
}

void ReminderEngine::addRule(std::unique_ptr<ReminderRule> rule) {
    require(rule != nullptr, "Пустое правило добавить нельзя");
    const auto duplicate = std::find_if(
        rules_.begin(),
        rules_.end(),
        [&rule](const auto& existing) {
            return existing->name() == rule->name();
        }
    );
    require(duplicate == rules_.end(), "Правило уже добавлено");
    rules_.push_back(std::move(rule));
}

std::size_t ReminderEngine::ruleCount() const noexcept {
    return rules_.size();
}

std::vector<Reminder> ReminderEngine::evaluate(
    const ApplicationState& state,
    const DateTime& now
) const {
    std::vector<Reminder> result;
    for (const auto& task : state.tasks) {
        for (const auto& rule : rules_) {
            if (rule->matches(task, now)) {
                result.push_back(rule->make(task, now));
            }
        }
    }
    std::stable_sort(
        result.begin(),
        result.end(),
        [](const Reminder& left, const Reminder& right) {
            return levelRank(left.level()) > levelRank(right.level());
        }
    );
    return result;
}

void ReminderEngine::swap(ReminderEngine& other) noexcept {
    rules_.swap(other.rules_);
}

void swap(ReminderEngine& left, ReminderEngine& right) noexcept {
    left.swap(right);
}

}  // namespace deadline
