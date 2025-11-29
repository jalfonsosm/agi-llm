# SDL3 as a dependency - fetched and built automatically if not found

include(FetchContent)

# First try to find SDL3 installed on the system
find_package(SDL3 QUIET CONFIG)

if(SDL3_FOUND)
    message(STATUS "Found system SDL3")
else()
    message(STATUS "SDL3 not found on system - fetching and building from source...")

    # Fetch SDL3 from GitHub - use stable release tag
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.14
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )

    # Configure SDL3 build options - static only
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

    # Disable subsystems we don't need to speed up build
    set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
    set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
    set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)
    set(SDL_POWER OFF CACHE BOOL "" FORCE)
    set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
    set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)

    # Don't treat warnings as errors for SDL3
    set(SDL_WERROR OFF CACHE BOOL "" FORCE)

    # On Linux, disable X11 optional features that require extra dev packages
    if(UNIX AND NOT APPLE)
        set(SDL_X11_XCURSOR OFF CACHE BOOL "" FORCE)
        set(SDL_X11_XDBE OFF CACHE BOOL "" FORCE)
        set(SDL_X11_XFIXES OFF CACHE BOOL "" FORCE)
        set(SDL_X11_XINPUT OFF CACHE BOOL "" FORCE)
        set(SDL_X11_XRANDR OFF CACHE BOOL "" FORCE)
        set(SDL_X11_XSCRNSAVER OFF CACHE BOOL "" FORCE)
        set(SDL_X11_XSHAPE OFF CACHE BOOL "" FORCE)
        set(SDL_X11_XTEST OFF CACHE BOOL "" FORCE)
        set(SDL_X11_XSYNC OFF CACHE BOOL "" FORCE)
        # Disable XInput2 to avoid build issues with missing dev libs
        set(SDL_XINPUT OFF CACHE BOOL "" FORCE)
    endif()

    FetchContent_MakeAvailable(SDL3)

    message(STATUS "SDL3 fetched and configured for static build")
endif()

# Prefer static linking
if(TARGET SDL3::SDL3-static)
    set(SDL3_TARGET SDL3::SDL3-static)
    message(STATUS "Using SDL3 static library")
elseif(TARGET SDL3-static)
    # When built via FetchContent, target might not have namespace
    set(SDL3_TARGET SDL3-static)
    message(STATUS "Using SDL3 static library (FetchContent)")
elseif(TARGET SDL3::SDL3)
    set(SDL3_TARGET SDL3::SDL3)
    message(STATUS "Using SDL3 shared library")
elseif(TARGET SDL3)
    set(SDL3_TARGET SDL3)
    message(STATUS "Using SDL3 (FetchContent)")
else()
    message(FATAL_ERROR "SDL3 target not found!")
endif()
