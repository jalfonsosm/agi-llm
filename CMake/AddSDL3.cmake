# SDL3 as a dependency - fetched and built automatically if not found

include(FetchContent)

# First try to find SDL3 installed on the system
find_package(SDL3 QUIET CONFIG)

# Check if SDL3 was found AND provides usable targets
set(SDL3_USABLE FALSE)
if(SDL3_FOUND)
    # Verify that at least one usable target exists
    if(TARGET SDL3::SDL3-static OR TARGET SDL3-static OR TARGET SDL3::SDL3-shared OR TARGET SDL3::SDL3 OR TARGET SDL3)
        set(SDL3_USABLE TRUE)
        message(STATUS "Found system SDL3 with usable targets")
    else()
        message(STATUS "Found system SDL3 but no usable targets - will build from source")
    endif()
endif()

if(NOT SDL3_USABLE)
    message(STATUS "SDL3 not found on system or not usable - fetching and building from source...")

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
    # Enable install to generate proper CMake config files for find_package
    set(SDL_INSTALL ON CACHE BOOL "" FORCE)

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

    # Create a simple SDL3Config.cmake that exposes the targets
    # This allows SDL3_ttf to find SDL3 via find_package()
    set(SDL3_CONFIG_DIR "${CMAKE_BINARY_DIR}/sdl3_config")
    file(MAKE_DIRECTORY "${SDL3_CONFIG_DIR}")

    file(WRITE "${SDL3_CONFIG_DIR}/SDL3Config.cmake" "
# SDL3 Config file for FetchContent usage
set(SDL3_FOUND TRUE)
set(SDL3_VERSION \"3.2.14\")
set(SDL3_INCLUDE_DIRS \"${sdl3_SOURCE_DIR}/include\" \"${sdl3_BINARY_DIR}/include\")
set(SDL3_LIBRARIES SDL3-static)

# Export targets if they don't already exist
# Only create aliases if the underlying target exists
if(TARGET SDL3-static)
    if(NOT TARGET SDL3::SDL3-static)
        add_library(SDL3::SDL3-static ALIAS SDL3-static)
    endif()

    if(NOT TARGET SDL3::SDL3)
        add_library(SDL3::SDL3 ALIAS SDL3-static)
    endif()
endif()
")

    # Create version file
    file(WRITE "${SDL3_CONFIG_DIR}/SDL3ConfigVersion.cmake" "
# SDL3 version file for FetchContent usage
set(PACKAGE_VERSION \"3.2.14\")

# Check whether the requested PACKAGE_FIND_VERSION is compatible
if(\"\${PACKAGE_VERSION}\" VERSION_LESS \"\${PACKAGE_FIND_VERSION}\")
    set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
    if(\"\${PACKAGE_VERSION}\" VERSION_EQUAL \"\${PACKAGE_FIND_VERSION}\")
        set(PACKAGE_VERSION_EXACT TRUE)
    endif()
endif()
")

    message(STATUS "SDL3 fetched and configured for static build")
    message(STATUS "Created SDL3Config.cmake at: ${SDL3_CONFIG_DIR}")
endif()

# Prefer static linking
if(TARGET SDL3::SDL3-static)
    set(SDL3_TARGET SDL3::SDL3-static)
    message(STATUS "Using SDL3 static library")
elseif(TARGET SDL3-static)
    # When built via FetchContent, target might not have namespace
    set(SDL3_TARGET SDL3-static)
    message(STATUS "Using SDL3 static library (FetchContent)")
elseif(TARGET SDL3::SDL3-shared)
    set(SDL3_TARGET SDL3::SDL3-shared)
    message(STATUS "Using SDL3 shared library (system)")
elseif(TARGET SDL3::SDL3)
    set(SDL3_TARGET SDL3::SDL3)
    message(STATUS "Using SDL3 shared library")
elseif(TARGET SDL3)
    set(SDL3_TARGET SDL3)
    message(STATUS "Using SDL3 (FetchContent)")
else()
    # Debug: List all available targets
    message(STATUS "SDL3_FOUND: ${SDL3_FOUND}")
    if(SDL3_FOUND)
        get_property(imported_targets DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY IMPORTED_TARGETS)
        message(STATUS "Available imported targets: ${imported_targets}")
    endif()
    message(FATAL_ERROR "SDL3 target not found! Available targets listed above.")
endif()
