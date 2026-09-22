#include "deadline/demo.hpp"

#include <memory>
#include <utility>
#include <vector>

namespace deadline {
namespace {

TaskData dataFor(
    const std::string& title,
    const std::string& projectId,
    const DateTime& createdAt,
    const DateTime& deadline,
    const int priority,
    const int estimate,
    const std::string& description = {}
) {
    TaskData data;
    data.title = title;
    data.projectId = projectId;
    data.createdAt = createdAt;
    data.periodStart = createdAt;
    data.deadline = deadline;
    data.priority = priority;
    data.estimate = TimeInterval(estimate);
    data.description = description;
    return data;
}

}  // namespace

DemoScenarioBuilder::DemoScenarioBuilder(const DateTime now) noexcept
    : now_(now) {}

void DemoScenarioBuilder::populate(TaskService& service) const {
    require(
        service.state().projects.empty() && service.tasks().empty(),
        "Демонстрационные данные добавляются только в пустое состояние"
    );

    ProjectService projects(service);
    const auto study = projects.add("Учёба");
    const auto personal = projects.add("Личные планы");
    const auto created = now_.addDays(-5);

    service.addTask(std::make_unique<DeadlineTask>(dataFor(
        "Записаться на консультацию",
        study,
        created,
        now_.addDays(-1),
        2,
        10,
        "Уточнить требования к защите"
    )));

    service.addTask(std::make_unique<DeadlineTask>(dataFor(
        "Отправить лабораторную работу",
        study,
        created,
        now_.addHours(6),
        3,
        45
    )));

    service.addTask(std::make_unique<RecurringTask>(
        dataFor(
            "Повторить английский",
            personal,
            created,
            now_.addDays(2),
            2,
            30
        ),
        3
    ));

    service.addTask(std::make_unique<ChecklistTask>(
        dataFor(
            "Подготовить проект по ООП",
            study,
            created,
            now_.addDays(4),
            3,
            240
        ),
        std::vector<ChecklistItem>{
            ChecklistItem(IdGenerator::create(), "Проверить архитектуру", true),
            ChecklistItem(IdGenerator::create(), "Запустить тесты", true),
            ChecklistItem("Провести репетицию защиты")
        }
    ));

    service.addTask(std::make_unique<DeadlineTask>(dataFor(
        "Составить план на следующую неделю",
        personal,
        created,
        now_.addDays(6),
        1,
        20
    )));

    auto completed = std::make_unique<DeadlineTask>(dataFor(
        "Изучить требования к проекту",
        study,
        created,
        now_.addDays(-2),
        2,
        35
    ));
    const auto completedId = completed->id();
    service.addTask(std::move(completed));
    service.complete(completedId);
}

}  // namespace deadline
