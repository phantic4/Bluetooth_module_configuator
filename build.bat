@echo off
setlocal
cd /d "%~dp0"
cmake -S . -B build -A x64
if errorlevel 1 exit /b 1
cmake --build build --config Release
if errorlevel 1 exit /b 1
echo.
echo Built: %CD%\build\Release\bluetooth_module_configurator.exe
