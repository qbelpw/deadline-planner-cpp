#include "deadline/ui/console_ui.hpp"

#include <iomanip>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include "deadline/reminders.hpp"
#include "deadline/exporters.hpp"

namespace deadline::ui {
namespace {

std::string typeLabel(const TaskType type) {
    switch (type) {
        case TaskType::deadline:
            return "дедлайн";
        case TaskType::recurring:
            return "повтор";
        case TaskType::checklist:
            return "чек-лист";
    }
    return "неизвестно";
}

std::string projectLabel(
    const ApplicationState& state,
    const std::string& identifier
) {
    const auto found = state.projects.find(identifier);
    return found == state.projects.end()
        ? "Неизвестный проект"
        : found->second.title();
}

void printTable(
    std::ostream& output,
    const ReportResult<ReportRow>& result
) {
    for (const auto& column : result.columns()) {
        output << std::left << std::setw(25) << column;
    }
    output << '\n' << std::string(result.columns().size() * 25, '-') << '\n';
    for (const auto& row : result.rows()) {
        for (const auto& value : row) {
            output << std::left << std::setw(25) << value;
        }
        output << '\n';
    }
    if (result.rows().empty()) {
        output << "Нет данных.\n";
    }
}

}  // namespace

ConsoleUi::ConsoleUi(
    TaskService& tasks,
    ProjectService& projects,
    std::istream& input,
    std::ostream& output
) noexcept
    : tasks_(tasks),
      projects_(projects),
      input_(input),
      output_(output) {}

void ConsoleUi::run() {
    output_ << "\nDEADLINE PLANNER — C++20\n";
    bool running = true;
    while (running && input_) {
        printMenu();
        const auto command = readLine("Команда: ");
        try {
            if (command == "1") {
                listTasks();
            } else if (command == "2") {
                addTask();
            } else if (command == "3") {
                completeTask();
            } else if (command == "4") {
                toggleChecklistItem();
            } else if (command == "5") {
                showReports();
            } else if (command == "6") {
                addProject();
            } else if (command == "7") {
                exportTasks();
            } else if (command == "0" || input_.eof()) {
                running = false;
            } else {
                output_ << "Неизвестная команда.\n";
            }
        } catch (const std::exception& error) {
            output_ << "Ошибка: " << error.what() << "\n";
        }
    }
    output_ << "Данные сохранены. До свидания!\n";
}

void ConsoleUi::printMenu() const {
    output_
        << "\n1. Список задач\n"
        << "2. Добавить задачу\n"
        << "3. Выполнить задачу\n"
        << "4. Изменить пункт чек-листа\n"
        << "5. Отчёты\n"
        << "6. Добавить проект\n"
        << "7. Экспорт в CSV и Markdown\n"
        << "0. Выход\n";
}

void ConsoleUi::listTasks() const {
    const auto tasks = tasks_.find(TaskFilter{});
    output_ << "\nЗадачи: " << tasks.size() << "\n";
    if (tasks.empty()) {
        output_ << "Список пуст.\n";
        return;
    }
    std::size_t index = 1;
    const auto now = DateTime::now();
    for (const auto reference : tasks) {
        const auto& task = reference.get();
        const auto scale = task.scale(now);
        output_ << index++ << ". [" << (task.done() ? 'x' : ' ')
                << "] " << task.title() << " | "
                << projectLabel(tasks_.state(), task.projectId()) << " | "
                << task.deadline().format() << " | " << scale.label << " | "
                << typeLabel(task.type()) << "\n"
                << "   id: " << task.id() << "\n";
    }
}

void ConsoleUi::addTask() {
    TaskDraft draft;
    draft.title = readLine("Название: ");
    draft.projectId = chooseProject();
    draft.deadline = DateTime::parse(
        readLine("Срок (ГГГГ-ММ-ДД ЧЧ:ММ): ")
    );
    draft.priority = readInt("Приоритет 1..3: ", 1, 3);
    draft.estimate = TimeInterval(
        readInt("Оценка в минутах: ", 1, 100000)
    );
    draft.description = readLine("Описание (можно пустое): ");
    const auto type = readInt(
        "Тип: 1 — дедлайн, 2 — повтор, 3 — чек-лист: ",
        1,
        3
    );
    draft.type = static_cast<TaskType>(type - 1);
    if (draft.type == TaskType::recurring) {
        draft.intervalDays = readInt("Интервал в днях: ", 1, 3650);
    }
    if (draft.type == TaskType::checklist) {
        output_ << "Пункты чек-листа, пустая строка завершает ввод.\n";
        while (true) {
            const auto item = readLine("Пункт: ");
            if (trim(item).empty()) {
                break;
            }
            draft.checklistItems.push_back(item);
        }
    }
    const auto id = tasks_.addTask(draft);
    output_ << "Задача создана, id: " << id << "\n";
}

void ConsoleUi::completeTask() {
    listTasks();
    tasks_.complete(readLine("id задачи: "));
    output_ << "Операция выполнена.\n";
}

void ConsoleUi::toggleChecklistItem() {
    listTasks();
    const auto taskId = readLine("id чек-листа: ");
    const auto& task = tasks_.find(taskId);
    require(
        task.type() == TaskType::checklist,
        "Выбрана задача другого типа"
    );
    const auto& checklist = static_cast<const ChecklistTask&>(task);
    for (const auto& item : checklist.items()) {
        output_ << "[" << (item.done() ? 'x' : ' ') << "] "
                << item.text() << " | id: " << item.id() << "\n";
    }
    const auto itemId = readLine("id пункта: ");
    const auto done = readInt("1 — выполнен, 0 — активен: ", 0, 1);
    tasks_.setChecklistItem(taskId, itemId, done == 1);
    output_ << "Пункт изменён.\n";
}

void ConsoleUi::showReports() const {
    ReportService service(tasks_.state());
    const OverdueReport overdue;
    const WorkloadReport workload;
    const CompletionReport completion;
    const std::vector<const Report*> reports{
        &overdue,
        &workload,
        &completion
    };
    for (const auto* report : reports) {
        output_ << "\n" << report->title() << "\n";
        printTable(output_, service.generate(*report));
    }
    const ReminderEngine reminders;
    output_ << "\nУмные напоминания\n";
    const auto messages = reminders.evaluate(tasks_.state());
    if (messages.empty()) {
        output_ << "Нет актуальных напоминаний.\n";
    }
    for (const auto& message : messages) {
        output_ << "[" << toString(message.level()) << "] "
                << message.message() << "\n";
    }
}

void ConsoleUi::exportTasks() const {
    const CsvTaskExporter csv;
    const MarkdownTaskExporter markdown;
    const ExportService service(tasks_.state());
    const std::vector<std::reference_wrapper<const StateExporter>> exporters{
        csv,
        markdown
    };
    const std::filesystem::path directory = "exports";
    std::filesystem::create_directories(directory);
    for (const auto& document : service.runAll(exporters)) {
        const auto path = directory / document.suggestedFileName();
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        require(output.good(), "Не удалось открыть файл экспорта");
        const unsigned char bom[]{0xEF, 0xBB, 0xBF};
        output.write(
            reinterpret_cast<const char*>(bom),
            static_cast<std::streamsize>(sizeof(bom))
        );
        output << document.content();
        require(output.good(), "Не удалось записать файл экспорта");
        output_ << "Создан файл: " << path.string() << "\n";
    }
}

void ConsoleUi::addProject() {
    const auto identifier = projects_.add(readLine("Название проекта: "));
    output_ << "Проект создан, id: " << identifier << "\n";
}

std::string ConsoleUi::readLine(const std::string& prompt) const {
    output_ << prompt;
    std::string value;
    std::getline(input_, value);
    return value;
}

int ConsoleUi::readInt(
    const std::string& prompt,
    const int minimum,
    const int maximum
) const {
    while (input_) {
        const auto value = readLine(prompt);
        try {
            std::size_t position = 0;
            const auto result = std::stoi(value, &position);
            if (position == value.size()
                && result >= minimum
                && result <= maximum) {
                return result;
            }
        } catch (const std::exception&) {
        }
        output_ << "Введите целое число от " << minimum
                << " до " << maximum << ".\n";
    }
    throw PlannerError("Ввод завершён");
}

std::string ConsoleUi::chooseProject() const {
    std::vector<std::reference_wrapper<const Project>> projects;
    for (const auto& [identifier, project] : tasks_.state().projects) {
        static_cast<void>(identifier);
        if (!project.archived()) {
            projects.emplace_back(project);
        }
    }
    require(!projects.empty(), "Сначала создайте проект");
    for (std::size_t index = 0; index < projects.size(); ++index) {
        output_ << index + 1 << ". " << projects[index].get().title() << "\n";
    }
    const auto selected = readInt(
        "Номер проекта: ",
        1,
        static_cast<int>(projects.size())
    );
    return projects[static_cast<std::size_t>(selected - 1)].get().id();
}

}  // namespace deadline::ui
