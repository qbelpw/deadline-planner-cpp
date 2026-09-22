#pragma once

#include <iosfwd>
#include <string>

#include "deadline/reports.hpp"
#include "deadline/services.hpp"

namespace deadline::ui {

class ConsoleUi final {
public:
    ConsoleUi(
        TaskService& tasks,
        ProjectService& projects,
        std::istream& input,
        std::ostream& output
    ) noexcept;

    void run();

private:
    void printMenu() const;
    void listTasks() const;
    void addTask();
    void completeTask();
    void toggleChecklistItem();
    void showReports() const;
    void exportTasks() const;
    void addProject();

    [[nodiscard]] std::string readLine(const std::string& prompt) const;
    [[nodiscard]] int readInt(
        const std::string& prompt,
        int minimum,
        int maximum
    ) const;
    [[nodiscard]] std::string chooseProject() const;

    TaskService& tasks_;
    ProjectService& projects_;
    std::istream& input_;
    std::ostream& output_;
};

}  // namespace deadline::ui
