#include "deadline/project.hpp"

#include <utility>

namespace deadline {

Project::Project()
    : Project("Новый проект") {}

Project::Project(std::string title)
    : id_(IdGenerator::create()),
      title_(validateTitle(title)) {}

Project::Project(
    std::string identifier,
    std::string title,
    const bool archived
)
    : id_(std::move(identifier)),
      title_(validateTitle(title)),
      archived_(archived) {
    require(IdGenerator::isValid(id_), "Некорректный идентификатор проекта");
}

const std::string& Project::id() const noexcept {
    return id_;
}

const std::string& Project::title() const noexcept {
    return title_;
}

bool Project::archived() const noexcept {
    return archived_;
}

void Project::rename(const std::string& title) {
    title_ = validateTitle(title);
}

void Project::setArchived(const bool archived) noexcept {
    archived_ = archived;
}

std::string Project::validateTitle(const std::string& title) {
    auto value = trim(title);
    require(!value.empty(), "Название проекта не должно быть пустым");
    require(value.size() <= 100, "Название проекта слишком длинное");
    return value;
}

}  // namespace deadline
