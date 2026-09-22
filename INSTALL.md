# Подробная установка и запуск

## Уже установленное окружение

На текущем компьютере настроены:

```text
C:\msys64\ucrt64\bin\g++.exe       GCC 16.2.0
C:\msys64\ucrt64\include\boost    Boost 1.92
C:\Program Files\CMake\bin\cmake.exe
```

Поэтому здесь достаточно открыть папку проекта и запустить `build.bat`.

## Установка на другом Windows-компьютере

### 1. Установите MSYS2

Скачайте MSYS2 с официального сайта и установите в стандартную папку
`C:\msys64`. Запустите **MSYS2 UCRT64** и обновите пакеты:

```bash
pacman -Syu
```

Если терминал попросит закрыться, откройте его снова и повторите:

```bash
pacman -Syu
```

### 2. Установите компилятор и Boost

В терминале **MSYS2 UCRT64** выполните:

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-make \
  mingw-w64-ucrt-x86_64-boost
```

Проект использует UCRT64. Нельзя смешивать библиотеки из папок `mingw64` и
`ucrt64`.

### 3. Установите CMake

Подходит CMake 3.24 или новее. Его можно установить отдельным Windows
установщиком или пакетом MSYS2 из предыдущего шага.

### 4. Скопируйте проект

Копировать нужно всю папку `deadline_planner_cpp`, сохраняя структуру
`include`, `src`, `tests`, `tools` и bat-файлы. Папку `build` можно не
копировать: она создастся заново.

## Сборка

Из Проводника запустите `build.bat`. Либо откройте PowerShell в папке проекта:

```powershell
.\build.bat
```

Скрипт выполняет эквивалентные команды:

```powershell
cmake -S . -B build -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe `
  -DCMAKE_MAKE_PROGRAM=C:/msys64/ucrt64/bin/mingw32-make.exe
cmake --build build --parallel
```

После успешной сборки исполняемые файлы появятся в `build\bin`.

## Демонстрационный запуск

Для защиты используйте:

```powershell
.\start_demo.bat
```

Что делает скрипт:

1. Переходит именно в папку проекта.
2. Проверяет наличие GUI exe и при необходимости собирает проект.
3. Удаляет только `data\demo.json`.
4. Запускает GUI с параметром `--reset-demo`.
5. Создаёт одинаковый набор данных для каждой репетиции.

Обычный режим:

```powershell
.\start_gui.bat
```

Он использует `data\planner.json` и сохраняет внесённые вручную задачи между
запусками.

## Консольная версия

```powershell
.\start_console.bat
```

Команды меню позволяют просмотреть и создать задачи, завершить задачу,
изменить пункт чек-листа, открыть отчёты, создать проект и экспортировать
данные.

Прямой запуск со своим файлом:

```powershell
.\build\bin\deadline_planner_cli.exe `
  --data data\my_data.json --demo
```

Поддерживаемые параметры:

```text
--data PATH    выбрать JSON-файл
--demo         заполнить пустое состояние демо-данными
--reset-demo   удалить выбранный файл и пересоздать демо
```

## Тесты

```powershell
.\tests.bat
```

Или через CTest:

```powershell
ctest --test-dir build --output-on-failure
```

Тестовый target компилируется с включённым `assert`, даже когда основная
сборка имеет тип Release.

## Производительность и память

```powershell
.\benchmark.bat
```

Benchmark создаёт 1000 задач, выполняет фильтрацию, отчёт и полный цикл
JSON-кодека. На Windows память измеряется через `GetProcessMemoryInfo`.

## Где лежат данные

```text
data/planner.json   обычный режим
data/demo.json      воспроизводимая демонстрация
exports/*.csv       экспорт для Excel
exports/*.md        экспорт для Markdown
```

JSON содержит версию формата, проекты, конкретные типы задач, поля чек-листов,
параметры повторения и историю действий.

## Если приложение не запускается

### Не найден DLL

Убедитесь, что в `PATH` есть:

```text
C:\msys64\ucrt64\bin
```

Bat-файлы добавляют её автоматически. При прямом запуске exe из другой папки
Windows может не найти runtime GCC.

### CMake не найден

Добавьте путь к CMake в `PATH` или запускайте CMake по полному пути:

```text
C:\Program Files\CMake\bin\cmake.exe
```

### Boost не найден

Проверьте наличие файла:

```text
C:\msys64\ucrt64\include\boost\version.hpp
```

Если его нет, повторите установку пакета Boost в MSYS2 UCRT64.

### JSON повреждён

Не редактируйте рабочий JSON во время работы приложения. Для демонстрации
просто снова запустите `start_demo.bat`: он пересоздаёт только демо-файл.

### Русский текст отображается неверно

GUI использует Unicode API Win32. Для консоли bat-файл включает кодовую
страницу UTF-8 командой `chcp 65001`.
