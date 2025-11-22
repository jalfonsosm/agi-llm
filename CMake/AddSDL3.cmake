# SDL3 supplies an SDL3Config.cmake file which should be picked up by cmake

find_package(SDL3 REQUIRED)

# SDL3 provides modern CMake targets directly
# SDL3::SDL3 is the main target (no separate SDL3main in SDL3)
