cmake_minimum_required(VERSION 3.14)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OS X deployment version")

project(NAGI
    VERSION "2.0.7"
    DESCRIPTION "New Adventure Game Interpreter"
    LANGUAGES C CXX
)

# LLM Options
option(NAGI_LLM_ENABLE_LLAMACPP "Enable llama.cpp backend" ON)
option(NAGI_LLM_ENABLE_BITNET "Enable BitNet backend" OFF)
option(NAGI_LLM_ENABLE_CLOUD_API "Enable cloud API backend" OFF)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

# Check and install dependencies automatically
include(CheckDependencies)
check_and_install_dependencies()


# Interprocedural optimisation
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_not_supported_reason)
if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE          TRUE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO   TRUE)
else()
    message(STATUS "Interprocedural optimisation (IPO/LTO) not supported: <${ipo_not_supported_reason}>")
endif()

if(MSVC)
    add_compile_options(/MP /W3)
    add_compile_definitions(_CRT_SECURE_NO_DEPRECATE)
    add_compile_definitions(_CRT_NONSTDC_NO_DEPRECATE)

    # Force static runtime library for all configurations
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(/MTd)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")
    else()
        add_compile_options(/MT)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
    endif()
else()
    add_compile_options(
        -fsigned-char 
        -fno-strict-aliasing 
        -fwrapv
        -Wall
        -Wextra
        -Wcast-align
        -Wcast-qual
        -Wno-uninitialized
        -Wendif-labels
        -Wfloat-equal
        -Wformat
        -Wformat-security
        -Winit-self
        -Winline
        -Wmissing-noreturn
        -Wmissing-declarations
        -Wpointer-arith
        -Wshadow
        -Wundef
        -Wunreachable-code
        -Wunused-result
        -Wwrite-strings
        -Werror=write-strings
        -Werror=implicit-function-declaration
        -Werror=unused-result
    )
    add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wbad-function-cast>)
    add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wdeclaration-after-statement>)
    add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wno-missing-prototypes>)
    add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wold-style-definition>)
    add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes>)
endif()

# Fetch and configure SDL3 (before adding src so it's available)
include(AddSDL3)
include(AddSDL3_ttf)

# Add NAGI-LLM library (abstract LLM interface)
add_subdirectory(lib/nagi-llm)

add_subdirectory(src)
