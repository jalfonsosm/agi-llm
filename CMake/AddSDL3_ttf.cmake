# SDL3_ttf as a dependency - fetched and built automatically if not found

include(FetchContent)

# First try to find SDL3_ttf installed on the system
find_package(SDL3_ttf QUIET CONFIG)

if(SDL3_ttf_FOUND)
    message(STATUS "Found system SDL3_ttf")
else()
    message(STATUS "SDL3_ttf not found on system - fetching and building from source...")

    # Fetch SDL3_ttf from GitHub - use main branch (SDL3 version)
    FetchContent_Declare(
        SDL3_ttf
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
        GIT_TAG main
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )

    # Configure SDL3_ttf build options - static only
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SDL3TTF_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL3TTF_STATIC ON CACHE BOOL "" FORCE)
    set(SDL3TTF_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL3TTF_VENDORED ON CACHE BOOL "" FORCE)  # Use bundled FreeType
    set(SDL3TTF_INSTALL OFF CACHE BOOL "" FORCE)

    # Don't treat warnings as errors for SDL3_ttf
    set(SDL3TTF_WERROR OFF CACHE BOOL "" FORCE)

    # Force static runtime library (/MT) for MSVC
    if(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)
    endif()

    # Key: Point to SDL3's config directory
    # This allows SDL3_ttf's find_package(SDL3) to find our generated SDL3Config.cmake
    if(DEFINED SDL3_CONFIG_DIR)
        # Use the SDL3 config dir from AddSDL3.cmake
        set(CMAKE_PREFIX_PATH "${SDL3_CONFIG_DIR};${CMAKE_PREFIX_PATH}" CACHE STRING "" FORCE)
        message(STATUS "Added SDL3 config dir to CMAKE_PREFIX_PATH: ${SDL3_CONFIG_DIR}")
    endif()

    FetchContent_MakeAvailable(SDL3_ttf)

    message(STATUS "SDL3_ttf fetched and configured for static build")
endif()

# Prefer static linking
if(TARGET SDL3_ttf::SDL3_ttf-static)
    set(SDL3_TTF_TARGET SDL3_ttf::SDL3_ttf-static)
    message(STATUS "Using SDL3_ttf static library")
elseif(TARGET SDL3_ttf-static)
    # When built via FetchContent, target might not have namespace
    set(SDL3_TTF_TARGET SDL3_ttf-static)
    message(STATUS "Using SDL3_ttf static library (FetchContent)")
elseif(TARGET SDL3_ttf::SDL3_ttf)
    set(SDL3_TTF_TARGET SDL3_ttf::SDL3_ttf)
    message(STATUS "Using SDL3_ttf shared library")
elseif(TARGET SDL3_ttf)
    set(SDL3_TTF_TARGET SDL3_ttf)
    message(STATUS "Using SDL3_ttf (FetchContent)")
else()
    message(FATAL_ERROR "SDL3_ttf target not found!")
endif()
