#include <filesystem>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#include "deadline/demo.hpp"
#include "deadline/ui/console_ui.hpp"

namespace {

struct Options final {
    std::filesystem::path data = "data/deadlines.json";
    bool demo = false;
    bool resetDemo = false;
};

Options parseOptions(const int argc, char* argv[]) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--demo") {
            options.demo = true;
        } else if (argument == "--reset-demo") {
            options.demo = true;
            options.resetDemo = true;
        } else if (argument == "--data" && index + 1 < argc) {
            options.data = argv[++index];
        } else {
            throw deadline::PlannerError("Неизвестный параметр: " + argument);
        }
    }
    return options;
}

}  // namespace

int main(const int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif
    try {
        const auto options = parseOptions(argc, argv);
        if (options.resetDemo) {
            std::error_code error;
            std::filesystem::remove(options.data, error);
        }
        deadline::JsonTaskRepository repository(options.data);
        deadline::TaskService tasks(repository);
        deadline::ProjectService projects(tasks);
        if (tasks.state().projects.empty()) {
            if (options.demo) {
                deadline::DemoScenarioBuilder{}.populate(tasks);
            } else {
                projects.add("Учёба");
            }
        }
        deadline::ui::ConsoleUi ui(
            tasks,
            projects,
            std::cin,
            std::cout
        );
        ui.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Критическая ошибка: " << error.what() << '\n';
        return 1;
    }
}
