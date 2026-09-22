#pragma once

#include <string>

#include "deadline/common.hpp"

namespace deadline {

class Project final {
public:
    Project();
    explicit Project(std::string title);
    Project(std::string identifier, std::string title, bool archived);
    Project(const Project&) = default;
    Project(Project&&) noexcept = default;
    Project& operator=(const Project&) = default;
    Project& operator=(Project&&) noexcept = default;
    ~Project() = default;

    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& title() const noexcept;
    [[nodiscard]] bool archived() const noexcept;

    void rename(const std::string& title);
    void setArchived(bool archived) noexcept;

    bool operator==(const Project&) const noexcept = default;

private:
    static std::string validateTitle(const std::string& title);

    std::string id_;
    std::string title_;
    bool archived_ = false;
};

}  // namespace deadline
