@echo off
setlocal
cd /d "%~dp0"
chcp 65001 > nul
set "PATH=C:\msys64\ucrt64\bin;%PATH%"
if not exist build\bin\deadline_planner_gui.exe call build.bat
if errorlevel 1 exit /b 1
start "" build\bin\deadline_planner_gui.exe ^
  --data data\demo.json --reset-demo
