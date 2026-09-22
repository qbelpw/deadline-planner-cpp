#ifdef _WIN32

#include <windows.h>
#include <shellapi.h>

#include <filesystem>
#include <memory>
#include <string>

#include "deadline/demo.hpp"
#include "deadline/ui/win32_gui.hpp"

namespace {

std::string utf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const auto length = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        length,
        nullptr,
        nullptr
    );
    return result;
}

struct Options final {
    std::filesystem::path data = "data/deadlines.json";
    bool demo = false;
    bool resetDemo = false;
};

Options parseOptions() {
    Options options;
    int count = 0;
    const auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (arguments == nullptr) {
        return options;
    }
    for (int index = 1; index < count; ++index) {
        const std::wstring argument = arguments[index];
        if (argument == L"--demo") {
            options.demo = true;
        } else if (argument == L"--reset-demo") {
            options.demo = true;
            options.resetDemo = true;
        } else if (argument == L"--data" && index + 1 < count) {
            options.data = arguments[++index];
        } else {
            LocalFree(arguments);
            throw deadline::PlannerError(
                "Неизвестный параметр запуска: " + utf8(argument)
            );
        }
    }
    LocalFree(arguments);
    return options;
}

}  // namespace

int WINAPI wWinMain(
    HINSTANCE,
    HINSTANCE,
    PWSTR,
    int showCommand
) {
    try {
        const auto options = parseOptions();
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
        return deadline::ui::runWin32Gui(tasks, projects, showCommand);
    } catch (const std::exception& error) {
        MessageBoxA(
            nullptr,
            error.what(),
            "Deadline Planner: ошибка запуска",
            MB_OK | MB_ICONERROR
        );
        return 1;
    }
}

#endif
