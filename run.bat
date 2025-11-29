@echo off
REM Run script for NAGI - Windows

setlocal

set SCRIPT_DIR=%~dp0
set NAGI_EXE=%SCRIPT_DIR%build\Release\nagi.exe

REM Check Release build first, then Debug
if not exist "%NAGI_EXE%" (
    set NAGI_EXE=%SCRIPT_DIR%build\Debug\nagi.exe
)
if not exist "%NAGI_EXE%" (
    set NAGI_EXE=%SCRIPT_DIR%build\nagi.exe
)

if not exist "%NAGI_EXE%" (
    echo Error: nagi.exe not found
    echo Run build.bat first to compile.
    exit /b 1
)

REM Game directory as argument, or current directory
set GAME_DIR=%1
if "%GAME_DIR%"=="" set GAME_DIR=%CD%

echo Starting NAGI...
echo Game directory: %GAME_DIR%
echo.

cd /d "%GAME_DIR%"
"%NAGI_EXE%"

endlocal
