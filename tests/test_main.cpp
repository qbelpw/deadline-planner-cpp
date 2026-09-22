#include <cassert>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "deadline/demo.hpp"
#include "deadline/exporters.hpp"
#include "deadline/reports.hpp"
#include "deadline/reminders.hpp"
#include "deadline/statistics.hpp"
#include "deadline/ui/console_ui.hpp"

namespace {

using namespace deadline;

static_assert(std::is_abstract_v<Task>);
static_assert(std::is_abstract_v<TaskRepository>);
static_assert(std::is_abstract_v<Report>);
static_assert(std::has_virtual_destructor_v<Task>);
static_assert(std::has_virtual_destructor_v<TaskRepository>);
static_assert(std::has_virtual_destructor_v<Report>);
static_assert(std::is_final_v<DeadlineTask>);
static_assert(std::is_final_v<RecurringTask>);
static_assert(std::is_final_v<ChecklistTask>);
static_assert(std::is_abstract_v<ReminderRule>);
static_assert(std::has_virtual_destructor_v<ReminderRule>);
static_assert(std::is_final_v<OverdueReminderRule>);
static_assert(std::is_final_v<UrgentReminderRule>);
static_assert(std::is_final_v<HighPriorityReminderRule>);
static_assert(std::is_abstract_v<StateExporter>);
static_assert(std::has_virtual_destructor_v<StateExporter>);
static_assert(std::is_final_v<CsvTaskExporter>);
static_assert(std::is_final_v<MarkdownTaskExporter>);
static_assert(std::is_final_v<StatisticsService>);
static_assert(std::is_final_v<ExportService>);
static_assert(std::is_final_v<ReminderEngine>);
static_assert(std::is_copy_constructible_v<ReminderEngine>);
static_assert(std::is_base_of_v<Task, DeadlineTask>);
static_assert(std::is_base_of_v<Task, RecurringTask>);
static_assert(std::is_base_of_v<Task, ChecklistTask>);
static_assert(EntityHashTable<Task>::defaultBucketCount == 127);

const DateTime fixedNow = DateTime::parse("2026-09-22 12:00");

template<typename Function>
bool throwsPlannerError(Function&& function) {
    try {
        function();
        return false;
    } catch (const PlannerError&) {
        return true;
    }
}

TaskData taskData(
    const std::string& title,
    const std::string& projectId,
    const DateTime deadline
) {
    TaskData data;
    data.title = title;
    data.projectId = projectId;
    data.createdAt = fixedNow;
    data.periodStart = fixedNow;
    data.deadline = deadline;
    return data;
}

struct Fixture final {
    InMemoryTaskRepository repository;
    TaskService tasks;
    ProjectService projects;
    std::string projectId;

    Fixture()
        : tasks(repository, [] { return fixedNow; }),
          projects(tasks),
          projectId(projects.add("Учёба")) {}
};

void testDateAndInterval() {
    assert(fixedNow.format() == "2026-09-22 12:00");
    assert(fixedNow.minutesUntil(fixedNow.addHours(3)) == 180);
    assert(fixedNow.addDays(2).format() == "2026-09-24 12:00");
    assert((TimeInterval(20) + TimeInterval(40)).minutes() == 60);
    auto interval = TimeInterval(15);
    interval += TimeInterval(5);
    assert(interval.minutes() == 20);
    assert(throwsPlannerError([] { static_cast<void>(TimeInterval(-1)); }));
    assert(throwsPlannerError([] {
        static_cast<void>(DateTime::parse("22.09.2026"));
    }));

    const auto normal = DeadlineScale::calculate(
        fixedNow,
        fixedNow.addDays(10),
        fixedNow.addDays(5),
        false
    );
    assert(normal.elapsedShare > 0.49 && normal.elapsedShare < 0.51);
    const auto overdue = DeadlineScale::calculate(
        fixedNow,
        fixedNow.addDays(1),
        fixedNow.addDays(2),
        false
    );
    assert(overdue.urgency == DeadlineScale::Urgency::overdue);
}

void testPolymorphicTasks() {
    const auto project = IdGenerator::create();
    std::vector<std::unique_ptr<Task>> tasks;
    tasks.push_back(std::make_unique<DeadlineTask>(taskData(
        "Обычная задача",
        project,
        fixedNow.addDays(1)
    )));
    tasks.push_back(std::make_unique<RecurringTask>(
        taskData("Повтор", project, fixedNow.addDays(1)),
        3
    ));
    tasks.push_back(std::make_unique<ChecklistTask>(
        taskData("Чек-лист", project, fixedNow.addDays(2)),
        std::vector<ChecklistItem>{ChecklistItem("Пункт")}
    ));

    tasks[0]->complete(fixedNow.addHours(1));
    assert(tasks[0]->done());
    tasks[1]->complete(fixedNow.addDays(5));
    assert(!tasks[1]->done());
    const auto& recurring = static_cast<const RecurringTask&>(*tasks[1]);
    assert(recurring.completionCount() == 1);
    assert(fixedNow.addDays(5) < recurring.deadline());
    assert(throwsPlannerError([&] {
        tasks[2]->complete(fixedNow.addHours(1));
    }));
    auto& checklist = static_cast<ChecklistTask&>(*tasks[2]);
    checklist.setItemDone(checklist.items()[0].id(), true);
    tasks[2]->complete(fixedNow.addHours(1));
    assert(tasks[2]->done());

    const auto copy = tasks[1]->clone();
    assert(copy->equals(*tasks[1]));
    assert(copy.get() != tasks[1].get());
}

void testHashTableRuleOfFiveAndOperators() {
    const auto project = IdGenerator::create();
    auto first = std::make_unique<DeadlineTask>(taskData(
        "Первая",
        project,
        fixedNow.addDays(1)
    ));
    const auto firstId = first->id();
    DeadlineTask second(taskData(
        "Вторая",
        project,
        fixedNow.addDays(2)
    ));
    const auto secondId = second.id();

    EntityHashTable<Task> table(7);
    table << std::move(first) << second;
    assert(table.size() == 2);
    assert(table.contains(firstId));
    assert(table[secondId].title() == "Вторая");

    EntityHashTable<Task> copy = table;
    assert(copy == table);
    copy[firstId].rename("Изменённая копия");
    assert(table[firstId].title() == "Первая");
    assert(!(copy == table));

    EntityHashTable<Task> moved = std::move(copy);
    assert(moved.size() == 2);
    assert(copy.empty());
    const auto common = table && moved;
    assert(common.size() == 1);
    assert(common.contains(secondId));
    assert(moved.remove(firstId));
    assert(!moved.remove(firstId));
}

void testServicesOverloadsAndRollback() {
    Fixture fixture;
    const auto first = fixture.tasks.addTask(
        "Сдать проект",
        fixedNow.addDays(2),
        fixture.projectId
    );
    const auto second = fixture.tasks.addTask(
        "Подготовить речь",
        fixedNow.addDays(3),
        fixture.projectId,
        3,
        TimeInterval(90),
        "Репетиция на десять минут"
    );
    assert(fixture.tasks.tasks().size() == 2);
    assert(fixture.tasks.find(second).priority() == 3);

    TaskFilter filter;
    filter.search = "речь";
    assert(fixture.tasks.find(filter).size() == 1);
    fixture.tasks.reschedule(first, fixedNow.addDays(4));
    assert(fixture.tasks.find(first).deadline() == fixedNow.addDays(4));

    const auto before = fixture.tasks.state();
    fixture.repository.failNextSave();
    assert(throwsPlannerError([&] { fixture.tasks.complete(first); }));
    assert(fixture.tasks.state() == before);

    fixture.tasks.complete(first);
    assert(fixture.tasks.find(first).done());
    assert(throwsPlannerError([&] {
        fixture.projects.archive(fixture.projectId);
    }));
    fixture.tasks.remove(second);
    fixture.projects.archive(fixture.projectId);
    assert(fixture.tasks.state().projects.at(fixture.projectId).archived());
}

void testChecklistService() {
    Fixture fixture;
    TaskDraft draft;
    draft.type = TaskType::checklist;
    draft.title = "Защита";
    draft.projectId = fixture.projectId;
    draft.deadline = fixedNow.addDays(1);
    draft.checklistItems = {"Слайды", "Демонстрация"};
    const auto identifier = fixture.tasks.addTask(draft);
    const auto& created = static_cast<const ChecklistTask&>(
        fixture.tasks.find(identifier)
    );
    const auto firstItem = created.items()[0].id();
    const auto secondItem = created.items()[1].id();
    fixture.tasks.setChecklistItem(identifier, firstItem, true);
    fixture.tasks.setChecklistItem(identifier, secondItem, true);
    fixture.tasks.complete(identifier);
    assert(fixture.tasks.find(identifier).done());
}

void testRepositoryRoundTrip() {
    Fixture fixture;
    fixture.tasks.addTask(
        "Сериализация",
        fixedNow.addDays(2),
        fixture.projectId
    );
    const auto encoded = StateCodec::encode(fixture.tasks.state());
    const auto decoded = StateCodec::decode(encoded);
    assert(decoded == fixture.tasks.state());

    const auto file = std::filesystem::temp_directory_path()
        / "deadline_planner_cpp_test.json";
    std::error_code ignored;
    std::filesystem::remove(file, ignored);
    {
        JsonTaskRepository repository(file);
        repository.save(fixture.tasks.state());
        assert(repository.load() == fixture.tasks.state());
    }
    std::filesystem::remove(file, ignored);
}

void testReportsAndDynamicDispatch() {
    Fixture fixture;
    TaskData overdueData = taskData(
        "Просроченная",
        fixture.projectId,
        fixedNow.addDays(1)
    );
    overdueData.createdAt = fixedNow.addDays(-2);
    overdueData.periodStart = overdueData.createdAt;
    overdueData.deadline = fixedNow.addDays(-1);
    fixture.tasks.addTask(std::make_unique<DeadlineTask>(overdueData));
    fixture.tasks.addTask(
        "Будущая",
        fixedNow.addDays(3),
        fixture.projectId
    );

    ReportService service(fixture.tasks.state());
    const std::vector<std::unique_ptr<Report>> reports = [] {
        std::vector<std::unique_ptr<Report>> value;
        value.push_back(std::make_unique<OverdueReport>());
        value.push_back(std::make_unique<WorkloadReport>());
        value.push_back(std::make_unique<CompletionReport>());
        return value;
    }();
    assert(service.generate(*reports[0], fixedNow).size() == 1);
    assert(service.generate(*reports[1], fixedNow).size() == 2);
    assert(service.generate(*reports[2], fixedNow).size() == 0);
}

void testRemindersAndStatistics() {
    Fixture fixture;
    TaskData overdueData = taskData(
        "Просроченная важная задача",
        fixture.projectId,
        fixedNow.addDays(1)
    );
    overdueData.createdAt = fixedNow.addDays(-4);
    overdueData.periodStart = overdueData.createdAt;
    overdueData.deadline = fixedNow.addDays(-1);
    fixture.tasks.addTask(std::make_unique<DeadlineTask>(overdueData));

    TaskDraft urgent;
    urgent.title = "Срочная задача";
    urgent.projectId = fixture.projectId;
    urgent.deadline = fixedNow.addHours(4);
    urgent.priority = 3;
    fixture.tasks.addTask(urgent);

    const ReminderEngine engine;
    const auto reminders = engine.evaluate(fixture.tasks.state(), fixedNow);
    assert(engine.ruleCount() == 3);
    assert(reminders.size() == 2);
    assert(reminders[0].level() == ReminderLevel::critical);
    assert(reminders[1].level() == ReminderLevel::warning);

    ReminderEngine copy = engine;
    assert(copy.ruleCount() == engine.ruleCount());
    const auto copyResult = copy.evaluate(fixture.tasks.state(), fixedNow);
    assert(copyResult == reminders);

    const StatisticsService statistics(fixture.tasks.state());
    const auto dashboard = statistics.calculate(fixedNow);
    assert(dashboard.active == 2);
    assert(dashboard.overdue == 1);
    assert(dashboard.dueSoon == 1);
    assert(dashboard.completed == 0);
    assert(dashboard.total() == 2);
    assert(dashboard.projects.size() == 1);
}

void testPolymorphicExporters() {
    Fixture fixture;
    fixture.tasks.addTask(
        "Задача, содержащая запятую",
        fixedNow.addDays(2),
        fixture.projectId
    );
    fixture.tasks.addTask(
        "Обычная задача",
        fixedNow.addDays(1),
        fixture.projectId
    );
    const CsvTaskExporter csv;
    const MarkdownTaskExporter markdown;
    const std::vector<std::reference_wrapper<const StateExporter>> exporters{
        csv,
        markdown
    };
    const ExportService service(fixture.tasks.state());
    const auto documents = service.runAll(exporters);
    assert(documents.size() == 2);
    assert(documents[0].suggestedFileName() == "deadline_tasks.csv");
    assert(documents[0].content().find(
        "\"Задача, содержащая запятую\""
    ) != std::string::npos);
    assert(documents[1].suggestedFileName() == "deadline_tasks.md");
    assert(documents[1].content().find("| Статус |") != std::string::npos);
    assert(documents[1].content().find("Обычная задача") != std::string::npos);
}

void testValidationBoundaries() {
    const auto projectId = IdGenerator::create();
    assert(IdGenerator::isValid(projectId));
    assert(!IdGenerator::isValid("not-a-uuid"));

    assert(throwsPlannerError([] { static_cast<void>(Project("   ")); }));
    assert(throwsPlannerError([] {
        static_cast<void>(ChecklistItem(""));
    }));
    assert(throwsPlannerError([] {
        static_cast<void>(UrgentReminderRule(0));
    }));
    assert(throwsPlannerError([] {
        static_cast<void>(HighPriorityReminderRule(31));
    }));

    auto invalidPriority = taskData(
        "Некорректный приоритет",
        projectId,
        fixedNow.addDays(1)
    );
    invalidPriority.priority = 4;
    assert(throwsPlannerError([&] {
        static_cast<void>(DeadlineTask(invalidPriority));
    }));

    auto invalidDeadline = taskData(
        "Некорректный срок",
        projectId,
        fixedNow.addDays(-1)
    );
    assert(throwsPlannerError([&] {
        static_cast<void>(DeadlineTask(invalidDeadline));
    }));

    assert(throwsPlannerError([&] {
        static_cast<void>(RecurringTask(
            taskData("Повтор", projectId, fixedNow.addDays(1)),
            0
        ));
    }));
    assert(throwsPlannerError([&] {
        static_cast<void>(ChecklistTask(
            taskData("Пустой чек-лист", projectId, fixedNow.addDays(1)),
            {}
        ));
    }));

    EntityHashTable<Task> table;
    auto task = std::make_unique<DeadlineTask>(taskData(
        "Уникальная задача",
        projectId,
        fixedNow.addDays(1)
    ));
    const auto copy = task->clone();
    table.add(std::move(task));
    assert(throwsPlannerError([&] { table.add(copy->clone()); }));
    assert(throwsPlannerError([&] {
        static_cast<void>(table.at("missing"));
    }));

    Fixture fixture;
    assert(throwsPlannerError([&] {
        fixture.tasks.addTask(
            "Чужой проект",
            fixedNow.addDays(1),
            IdGenerator::create()
        );
    }));
    assert(throwsPlannerError([&] {
        fixture.projects.add("Учёба");
    }));
}

void testAllTaskTypesPersistence() {
    Fixture fixture;

    TaskDraft recurring;
    recurring.type = TaskType::recurring;
    recurring.title = "Регулярное повторение";
    recurring.projectId = fixture.projectId;
    recurring.deadline = fixedNow.addDays(1);
    recurring.intervalDays = 5;
    const auto recurringId = fixture.tasks.addTask(recurring);
    fixture.tasks.complete(recurringId);

    TaskDraft checklist;
    checklist.type = TaskType::checklist;
    checklist.title = "Проверка перед защитой";
    checklist.projectId = fixture.projectId;
    checklist.deadline = fixedNow.addDays(2);
    checklist.priority = 3;
    checklist.checklistItems = {
        "Запустить тесты",
        "Открыть диаграмму классов",
        "Проверить демонстрационные данные"
    };
    const auto checklistId = fixture.tasks.addTask(checklist);
    const auto& createdChecklist = static_cast<const ChecklistTask&>(
        fixture.tasks.find(checklistId)
    );
    fixture.tasks.setChecklistItem(
        checklistId,
        createdChecklist.items()[0].id(),
        true
    );

    const auto tree = StateCodec::encode(fixture.tasks.state());
    const auto restored = StateCodec::decode(tree);
    assert(restored == fixture.tasks.state());
    assert(restored.tasks.size() == 2);

    const auto& restoredRecurring = static_cast<const RecurringTask&>(
        restored.tasks[recurringId]
    );
    assert(restoredRecurring.intervalDays() == 5);
    assert(restoredRecurring.completionCount() == 1);
    assert(!restoredRecurring.done());

    const auto& restoredChecklist = static_cast<const ChecklistTask&>(
        restored.tasks[checklistId]
    );
    assert(restoredChecklist.items().size() == 3);
    assert(restoredChecklist.items()[0].done());
    assert(!restoredChecklist.items()[1].done());
    assert(restoredChecklist.priority() == 3);

    assert(restored.history.size() == 4);
    assert(restored.history[0].action() == HistoryAction::created);
    assert(restored.history[1].action() == HistoryAction::repeated);
    assert(restored.history[2].action() == HistoryAction::created);
    assert(
        restored.history[3].action()
            == HistoryAction::checklistChanged
    );
}

void testEnumConversions() {
    assert(trim("  значение  ") == "значение");
    assert(trim("\t\n") == "");
    assert(throwsPlannerError([] {
        require(false, "контролируемая ошибка");
    }));
    require(true, "эта ошибка не должна возникнуть");

    const TaskType taskTypes[]{
        TaskType::deadline,
        TaskType::recurring,
        TaskType::checklist
    };
    for (const auto type : taskTypes) {
        assert(taskTypeFromString(toString(type)) == type);
    }

    const HistoryAction actions[]{
        HistoryAction::created,
        HistoryAction::completed,
        HistoryAction::repeated,
        HistoryAction::rescheduled,
        HistoryAction::checklistChanged,
        HistoryAction::removed
    };
    for (const auto action : actions) {
        assert(historyActionFromString(toString(action)) == action);
    }

    assert(toString(ReminderLevel::information) == "Информация");
    assert(toString(ReminderLevel::warning) == "Предупреждение");
    assert(toString(ReminderLevel::critical) == "Критично");
    assert(throwsPlannerError([] {
        static_cast<void>(taskTypeFromString("invalid"));
    }));
    assert(throwsPlannerError([] {
        static_cast<void>(historyActionFromString("invalid"));
    }));
}

void testDemoAndConsoleUi() {
    InMemoryTaskRepository repository;
    TaskService tasks(repository, [] { return fixedNow; });
    ProjectService projects(tasks);
    DemoScenarioBuilder(fixedNow).populate(tasks);
    assert(tasks.state().projects.size() == 2);
    assert(tasks.tasks().size() == 6);

    std::istringstream input("1\n5\n0\n");
    std::ostringstream output;
    ui::ConsoleUi console(tasks, projects, input, output);
    console.run();
    const auto text = output.str();
    assert(text.find("DEADLINE PLANNER") != std::string::npos);
    assert(text.find("Подготовить проект по ООП") != std::string::npos);
    assert(text.find("Просроченные задачи") != std::string::npos);
}

}  // namespace

int main() {
    testDateAndInterval();
    testPolymorphicTasks();
    testHashTableRuleOfFiveAndOperators();
    testServicesOverloadsAndRollback();
    testChecklistService();
    testRepositoryRoundTrip();
    testReportsAndDynamicDispatch();
    testRemindersAndStatistics();
    testPolymorphicExporters();
    testValidationBoundaries();
    testAllTaskTypesPersistence();
    testEnumConversions();
    testDemoAndConsoleUi();
    std::cout << "Все тесты Deadline Planner успешно пройдены.\n";
    return 0;
}
