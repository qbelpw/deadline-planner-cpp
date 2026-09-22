cmake_minimum_required(VERSION 3.24)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

project(DeadlinePlannerCpp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(Boost 1.70 REQUIRED)

add_library(deadline_core STATIC
    src/date_time.cpp
    src/project.cpp
    src/task.cpp
    src/history.cpp
    src/repository.cpp
    src/services.cpp
    src/reports.cpp
    src/reminders.cpp
    src/statistics.cpp
    src/exporters.cpp
    src/demo.cpp
)

target_include_directories(deadline_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        ${Boost_INCLUDE_DIRS}
)

target_compile_definitions(deadline_core
    PUBLIC BOOST_BIND_GLOBAL_PLACEHOLDERS
)

if(MSVC)
    target_compile_options(deadline_core PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(deadline_core PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    )
endif()

add_executable(deadline_planner_cli
    src/console_main.cpp
    src/console_ui.cpp
)
target_link_libraries(deadline_planner_cli PRIVATE deadline_core)

if(WIN32)
    add_executable(deadline_planner_gui WIN32
        src/gui_main.cpp
        src/win32_gui.cpp
    )
    target_compile_definitions(deadline_planner_gui PRIVATE UNICODE _UNICODE)
    if(MINGW)
        target_link_options(deadline_planner_gui PRIVATE -municode)
    endif()
    target_link_libraries(deadline_planner_gui PRIVATE
        deadline_core
        comctl32
        gdi32
        shell32
        user32
    )
endif()

add_executable(deadline_planner_tests
    tests/test_main.cpp
    src/console_ui.cpp
)
target_compile_definitions(deadline_planner_tests PRIVATE
    DEADLINE_PLANNER_TESTS
)
if(MSVC)
    target_compile_options(deadline_planner_tests PRIVATE /UNDEBUG)
else()
    target_compile_options(deadline_planner_tests PRIVATE -UNDEBUG)
endif()
target_link_libraries(deadline_planner_tests PRIVATE deadline_core)

add_executable(deadline_planner_benchmark tools/benchmark.cpp)
target_link_libraries(deadline_planner_benchmark PRIVATE deadline_core)
if(WIN32)
    target_link_libraries(deadline_planner_benchmark PRIVATE psapi)
endif()

enable_testing()
add_test(NAME deadline_planner_tests COMMAND deadline_planner_tests)
