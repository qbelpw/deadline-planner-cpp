#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/property_tree/ptree.hpp>

#include "deadline/entity_hash_table.hpp"
#include "deadline/history.hpp"
#include "deadline/project.hpp"
#include "deadline/task.hpp"

namespace deadline {

struct ApplicationState final {
    std::unordered_map<std::string, Project> projects;
    EntityHashTable<Task> tasks;
    std::vector<HistoryEntry> history;

    ApplicationState() = default;
    ApplicationState(const ApplicationState&) = default;
    ApplicationState(ApplicationState&&) noexcept = default;
    ApplicationState& operator=(const ApplicationState&) = default;
    ApplicationState& operator=(ApplicationState&&) noexcept = default;
    ~ApplicationState() = default;

    bool operator==(const ApplicationState& other) const noexcept {
        return projects == other.projects
            && tasks == other.tasks
            && history == other.history;
    }
};

class StateCodec final {
public:
    [[nodiscard]] static boost::property_tree::ptree encode(
        const ApplicationState& state
    );
    [[nodiscard]] static ApplicationState decode(
        const boost::property_tree::ptree& root
    );

    StateCodec() = delete;
};

class TaskRepository {
public:
    virtual ~TaskRepository() = default;
    [[nodiscard]] virtual ApplicationState load() const = 0;
    virtual void save(const ApplicationState& state) = 0;
};

class InMemoryTaskRepository final : public TaskRepository {
public:
    InMemoryTaskRepository() = default;
    explicit InMemoryTaskRepository(ApplicationState initial);
    InMemoryTaskRepository(const InMemoryTaskRepository&) = default;
    InMemoryTaskRepository(InMemoryTaskRepository&&) noexcept = default;
    InMemoryTaskRepository& operator=(
        const InMemoryTaskRepository&
    ) = default;
    InMemoryTaskRepository& operator=(
        InMemoryTaskRepository&&
    ) noexcept = default;
    ~InMemoryTaskRepository() override = default;

    [[nodiscard]] ApplicationState load() const override;
    void save(const ApplicationState& state) override;
    void failNextSave(bool enabled = true) noexcept;

private:
    ApplicationState state_;
    bool failNextSave_ = false;
};

class JsonTaskRepository final : public TaskRepository {
public:
    explicit JsonTaskRepository(std::filesystem::path path);
    JsonTaskRepository(const JsonTaskRepository&) = delete;
    JsonTaskRepository(JsonTaskRepository&&) noexcept = default;
    JsonTaskRepository& operator=(const JsonTaskRepository&) = delete;
    JsonTaskRepository& operator=(JsonTaskRepository&&) noexcept = default;
    ~JsonTaskRepository() override = default;

    [[nodiscard]] ApplicationState load() const override;
    void save(const ApplicationState& state) override;
    [[nodiscard]] const std::filesystem::path& path() const noexcept;

private:
    std::filesystem::path path_;
};

}  // namespace deadline
