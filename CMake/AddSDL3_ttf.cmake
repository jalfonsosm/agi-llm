include(FetchContent)

# Avoid network updates when sources are already present (important for offline builds)
set(FETCHCONTENT_UPDATES_DISCONNECTED_SDL3_TTF ON CACHE BOOL "" FORCE)

# SDL3_ttf as a dependency - fetched and built automatically to keep toolchain consistent
message(STATUS "Building SDL3_ttf from source with vendored dependencies...")

# Always fetch SDL3_ttf from source (system packages often link to newer macOS SDKs)
FetchContent_Declare(
    SDL3_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG main
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

# Configure SDL3_ttf build options - static only
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)              # Use bundled FreeType/Harfbuzz/PlutoSVG
set(SDLTTF_HARFBUZZ_VENDORED ON CACHE BOOL "" FORCE)
set(SDLTTF_FREETYPE_VENDORED ON CACHE BOOL "" FORCE)
set(SDLTTF_PLUTOSVG_VENDORED ON CACHE BOOL "" FORCE)
set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDLTTF_INSTALL OFF CACHE BOOL "" FORCE)

# Don't treat warnings as errors for SDL3_ttf
set(SDLTTF_WERROR OFF CACHE BOOL "" FORCE)

# Force static runtime library (/MT) for MSVC
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)
endif()

# Point SDL3_ttf at our SDL3 config so it resolves the dependency without a system install
if(DEFINED SDL3_CONFIG_DIR)
    set(CMAKE_PREFIX_PATH "${SDL3_CONFIG_DIR};${CMAKE_PREFIX_PATH}" CACHE STRING "" FORCE)
    message(STATUS "Added SDL3 config dir to CMAKE_PREFIX_PATH: ${SDL3_CONFIG_DIR}")
endif()

FetchContent_MakeAvailable(SDL3_ttf)

message(STATUS "SDL3_ttf fetched and configured for static build with vendored deps")

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
