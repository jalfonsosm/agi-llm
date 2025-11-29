@echo off
REM Build script for NAGI (SDL3) - Windows

setlocal

set SCRIPT_DIR=%~dp0
set BUILD_DIR=%SCRIPT_DIR%build
set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release

echo === NAGI Build Script ===
echo Build type: %BUILD_TYPE%
echo.

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

REM Configure
echo Configuring...
cmake .. -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if errorlevel 1 goto :error

REM Build
echo.
echo Building...
cmake --build . --config %BUILD_TYPE% --parallel
if errorlevel 1 goto :error

echo.
echo === Build complete ===
echo Executable: %BUILD_DIR%\%BUILD_TYPE%\nagi.exe
goto :end

:error
echo.
echo === Build FAILED ===
exit /b 1

:end
endlocal
