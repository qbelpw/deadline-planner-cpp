@echo off
setlocal
cd /d "%~dp0"
chcp 65001 > nul
set "PATH=C:\msys64\ucrt64\bin;C:\msys64\usr\bin;%PATH%"

cmake -S . -B build -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe ^
  -DCMAKE_MAKE_PROGRAM=C:/msys64/ucrt64/bin/mingw32-make.exe
if errorlevel 1 goto failed

cmake --build build --parallel
if errorlevel 1 goto failed

echo.
echo Сборка завершена. Исполняемые файлы находятся в build\bin.
exit /b 0

:failed
echo.
echo Сборка завершилась с ошибкой.
exit /b 1
