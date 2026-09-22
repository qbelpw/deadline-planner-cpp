#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

#include "deadline/reports.hpp"
#include "deadline/services.hpp"

namespace {

using namespace deadline;
using Timer = std::chrono::steady_clock;

template<typename Function>
double measureMilliseconds(Function&& function) {
    const auto started = Timer::now();
    function();
    return std::chrono::duration<double, std::milli>(
        Timer::now() - started
    ).count();
}

std::size_t workingSetBytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
            sizeof(counters))) {
        return static_cast<std::size_t>(counters.WorkingSetSize);
    }
#endif
    return 0;
}

}  // namespace

int main() {
    constexpr int taskCount = 1000;
    const auto now = DateTime::parse("2026-09-22 12:00");
    InMemoryTaskRepository repository;
    TaskService tasks(repository, [now] { return now; });
    ProjectService projects(tasks);
    const auto project = projects.add("Нагрузочный тест");
    const auto memoryBefore = workingSetBytes();

    const auto addTime = measureMilliseconds([&] {
        for (int index = 0; index < taskCount; ++index) {
            TaskDraft draft;
            draft.title = "Задача " + std::to_string(index);
            draft.projectId = project;
            draft.deadline = now.addDays(1 + index % 60);
            draft.priority = 1 + index % 3;
            draft.estimate = TimeInterval(15 + index % 180);
            if (index % 10 == 0) {
                draft.type = TaskType::recurring;
                draft.intervalDays = 7;
            } else if (index % 10 == 1) {
                draft.type = TaskType::checklist;
                draft.checklistItems = {"Подготовить", "Проверить"};
            }
            tasks.addTask(draft);
        }
    });

    std::size_t foundCount = 0;
    const auto filterTime = measureMilliseconds([&] {
        TaskFilter filter;
        filter.search = "Задача 9";
        foundCount = tasks.find(filter).size();
    });

    std::size_t reportRows = 0;
    const auto reportTime = measureMilliseconds([&] {
        ReportService service(tasks.state());
        const WorkloadReport report;
        reportRows = service.generate(report, now).size();
    });

    ApplicationState decoded;
    const auto codecTime = measureMilliseconds([&] {
        const auto tree = StateCodec::encode(tasks.state());
        decoded = StateCodec::decode(tree);
    });
    const auto memoryAfter = workingSetBytes();

    std::cout << std::fixed << std::setprecision(2)
              << "Deadline Planner — контроль производительности\n"
              << "Объектов: " << taskCount << "\n"
              << "Создание и транзакционное сохранение: "
              << addTime << " мс\n"
              << "Фильтрация: " << filterTime << " мс ("
              << foundCount << " совпадений)\n"
              << "Полиморфный отчёт: " << reportTime << " мс ("
              << reportRows << " строк)\n"
              << "JSON-кодек туда и обратно: " << codecTime << " мс\n"
              << "Состояние после декодирования: " << decoded.tasks.size()
              << " задач\n";
    if (memoryAfter >= memoryBefore && memoryBefore != 0) {
        const auto megabytes = static_cast<double>(
            memoryAfter - memoryBefore
        ) / (1024.0 * 1024.0);
        std::cout << "Прирост рабочего набора: " << megabytes << " МиБ\n";
    }
    return decoded.tasks.size() == taskCount ? 0 : 1;
}
