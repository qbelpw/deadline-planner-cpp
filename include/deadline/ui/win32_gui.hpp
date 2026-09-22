#pragma once

#include "deadline/services.hpp"

namespace deadline::ui {

int runWin32Gui(TaskService& tasks, ProjectService& projects, int showCommand);

}  // namespace deadline::ui
