#include "deadline/repository.hpp"

#include <filesystem>
#include <utility>

#include <boost/property_tree/json_parser.hpp>

namespace deadline {
namespace {

using boost::property_tree::ptree;

ptree encodeProject(const Project& project) {
    ptree node;
    node.put("id", project.id());
    node.put("title", project.title());
    node.put("archived", project.archived());
    return node;
}

ptree encodeTask(const Task& task) {
    ptree node;
    node.put("type", toString(task.type()));
    node.put("id", task.id());
    node.put("title", task.title());
    node.put("project_id", task.projectId());
    node.put("deadline", task.deadline().format());
    node.put("created_at", task.createdAt().format());
    node.put("period_start", task.periodStart().format());
    node.put("priority", task.priority());
    node.put("estimate_minutes", task.estimate().minutes());
    node.put("description", task.description());
    node.put(
        "completed_at",
        task.completedAt().has_value()
            ? task.completedAt()->format()
            : std::string{}
    );

    if (task.type() == TaskType::recurring) {
        const auto& recurring = static_cast<const RecurringTask&>(task);
        node.put("interval_days", recurring.intervalDays());
        node.put("completion_count", recurring.completionCount());
    } else if (task.type() == TaskType::checklist) {
        const auto& checklist = static_cast<const ChecklistTask&>(task);
        ptree items;
        for (const auto& item : checklist.items()) {
            ptree itemNode;
            itemNode.put("id", item.id());
            itemNode.put("text", item.text());
            itemNode.put("done", item.done());
            items.push_back({"", itemNode});
        }
        node.add_child("items", items);
    }
    return node;
}

ptree encodeHistory(const HistoryEntry& entry) {
    ptree node;
    node.put("id", entry.id());
    node.put("task_id", entry.taskId());
    node.put("action", toString(entry.action()));
    node.put("happened_at", entry.happenedAt().format());
    node.put("details", entry.details());
    return node;
}

TaskData decodeTaskData(const ptree& node) {
    TaskData data;
    data.id = node.get<std::string>("id");
    data.title = node.get<std::string>("title");
    data.projectId = node.get<std::string>("project_id");
    data.deadline = DateTime::parse(node.get<std::string>("deadline"));
    data.createdAt = DateTime::parse(node.get<std::string>("created_at"));
    data.periodStart = DateTime::parse(node.get<std::string>("period_start"));
    data.priority = node.get<int>("priority");
    data.estimate = TimeInterval(node.get<std::int64_t>("estimate_minutes"));
    data.description = node.get<std::string>("description", "");
    const auto completed = node.get<std::string>("completed_at", "");
    if (!completed.empty()) {
        data.completedAt = DateTime::parse(completed);
    }
    return data;
}

std::unique_ptr<Task> decodeTask(const ptree& node) {
    auto data = decodeTaskData(node);
    switch (taskTypeFromString(node.get<std::string>("type"))) {
        case TaskType::deadline:
            return std::make_unique<DeadlineTask>(std::move(data));
        case TaskType::recurring:
            return std::make_unique<RecurringTask>(
                std::move(data),
                node.get<int>("interval_days"),
                node.get<int>("completion_count", 0)
            );
        case TaskType::checklist: {
            std::vector<ChecklistItem> items;
            for (const auto& value : node.get_child("items")) {
                const auto& item = value.second;
                items.emplace_back(
                    item.get<std::string>("id"),
                    item.get<std::string>("text"),
                    item.get<bool>("done")
                );
            }
            return std::make_unique<ChecklistTask>(
                std::move(data),
                std::move(items)
            );
        }
    }
    throw PlannerError("Неизвестный тип задачи");
}

}  // namespace

boost::property_tree::ptree StateCodec::encode(
    const ApplicationState& state
) {
    ptree root;
    root.put("version", 1);

    ptree projects;
    for (const auto& [id, project] : state.projects) {
        static_cast<void>(id);
        projects.push_back({"", encodeProject(project)});
    }
    root.add_child("projects", projects);

    ptree tasks;
    for (const auto& task : state.tasks) {
        tasks.push_back({"", encodeTask(task)});
    }
    root.add_child("tasks", tasks);

    ptree history;
    for (const auto& entry : state.history) {
        history.push_back({"", encodeHistory(entry)});
    }
    root.add_child("history", history);
    return root;
}

ApplicationState StateCodec::decode(const ptree& root) {
    require(root.get<int>("version") == 1, "Неподдерживаемая версия данных");
    ApplicationState state;

    for (const auto& value : root.get_child("projects")) {
        const auto& node = value.second;
        Project project(
            node.get<std::string>("id"),
            node.get<std::string>("title"),
            node.get<bool>("archived", false)
        );
        const auto id = project.id();
        const auto [iterator, inserted] = state.projects.emplace(
            id,
            std::move(project)
        );
        static_cast<void>(iterator);
        require(inserted, "Повторяющийся идентификатор проекта");
    }

    for (const auto& value : root.get_child("tasks")) {
        auto task = decodeTask(value.second);
        require(
            state.projects.contains(task->projectId()),
            "Задача ссылается на отсутствующий проект"
        );
        state.tasks.add(std::move(task));
    }

    for (const auto& value : root.get_child("history")) {
        const auto& node = value.second;
        state.history.emplace_back(
            node.get<std::string>("id"),
            node.get<std::string>("task_id"),
            historyActionFromString(node.get<std::string>("action")),
            DateTime::parse(node.get<std::string>("happened_at")),
            node.get<std::string>("details", "")
        );
    }
    return state;
}

InMemoryTaskRepository::InMemoryTaskRepository(ApplicationState initial)
    : state_(std::move(initial)) {}

ApplicationState InMemoryTaskRepository::load() const {
    return state_;
}

void InMemoryTaskRepository::save(const ApplicationState& state) {
    if (failNextSave_) {
        failNextSave_ = false;
        throw PlannerError("Тестовый отказ сохранения");
    }
    state_ = state;
}

void InMemoryTaskRepository::failNextSave(const bool enabled) noexcept {
    failNextSave_ = enabled;
}

JsonTaskRepository::JsonTaskRepository(std::filesystem::path path)
    : path_(std::move(path)) {
    require(!path_.empty(), "Путь к файлу данных не задан");
}

ApplicationState JsonTaskRepository::load() const {
    if (!std::filesystem::exists(path_)) {
        return {};
    }
    try {
        ptree root;
        boost::property_tree::read_json(path_.string(), root);
        return StateCodec::decode(root);
    } catch (const PlannerError&) {
        throw;
    } catch (const std::exception& error) {
        throw PlannerError(
            "Не удалось загрузить данные: " + std::string(error.what())
        );
    }
}

void JsonTaskRepository::save(const ApplicationState& state) {
    try {
        const auto parent = path_.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent);
        }
        auto temporary = path_;
        temporary += ".tmp";
        boost::property_tree::write_json(
            temporary.string(),
            StateCodec::encode(state),
            std::locale(),
            true
        );
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
        std::filesystem::rename(temporary, path_);
    } catch (const std::exception& error) {
        throw PlannerError(
            "Не удалось сохранить данные: " + std::string(error.what())
        );
    }
}

const std::filesystem::path& JsonTaskRepository::path() const noexcept {
    return path_;
}

}  // namespace deadline
