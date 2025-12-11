@echo off
REM Build script for NAGI (SDL3) - Windows

setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "BUILD_DIR=%SCRIPT_DIR%build"
set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release

echo === NAGI Build Script ===
echo Build type: %BUILD_TYPE%
echo Script dir: %SCRIPT_DIR%
echo Build dir: %BUILD_DIR%
echo.

REM Always find and setup Visual Studio environment (don't trust inherited env)
REM This ensures we get the correct VS installation even if run from IDE
echo Searching for Visual Studio...
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: Visual Studio not found.
    echo Please run from "Developer Command Prompt" or install VS Build Tools.
    goto :error
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do (
    set VS_PATH=%%i
)

if not defined VS_PATH (
    echo ERROR: Visual Studio not found.
    echo Please run from "Developer Command Prompt".
    goto :error
)

echo Found Visual Studio at: %VS_PATH%
echo Initializing Visual Studio environment...
call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    echo ERROR: Failed to initialize Visual Studio environment
    goto :error
)

:build
REM Setup CUDA environment if available
if exist "%CUDA_PATH%\bin\nvcc.exe" (
    echo Found CUDA at: %CUDA_PATH%
    set "PATH=%CUDA_PATH%\bin;%PATH%"
    echo CUDA will be configured automatically by AddLlamaCpp.cmake
) else (
    echo WARNING: CUDA_PATH not set or CUDA not found
    echo Will compile without GPU acceleration
)

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

REM Setup Vulkan environment
if defined VULKAN_SDK (
    echo Found Vulkan SDK at: %VULKAN_SDK%
    set "CMAKE_PREFIX_PATH=%VULKAN_SDK%;%CMAKE_PREFIX_PATH%"
    set "LIB=%VULKAN_SDK%\Lib;%LIB%"
) else (
    echo WARNING: VULKAN_SDK not set
)

REM Configure with Ninja (better dependency handling)
echo.
echo Configuring...
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if errorlevel 1 goto :error

REM Build llama.cpp first (ExternalProject)
echo.
echo Building llama.cpp dependency...
cmake --build . --target llama_cpp --config %BUILD_TYPE%
if errorlevel 1 goto :error

REM Build main project
echo.
echo Building main project...
cmake --build . --config %BUILD_TYPE%
if errorlevel 1 goto :error

echo.
echo === Build complete ===
echo Executable: %BUILD_DIR%\src\nagi.exe
goto :end

:error
echo.
echo === Build FAILED ===
exit /b 1

:end
endlocal
