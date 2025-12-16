# Auto-install missing dependencies per platform

function(check_and_install_dependencies)
    message(STATUS "Checking platform-specific dependencies...")
    
    if(UNIX AND NOT APPLE)
        # Linux dependencies
        message(STATUS "Checking Linux dependencies...")
        
        # Check Vulkan (only on Linux - already handled in AddLlamaCpp.cmake)
        find_package(Vulkan QUIET)
        if(NOT Vulkan_FOUND)
            message(STATUS "Vulkan not found - attempting to install...")
            execute_process(
                COMMAND sudo apt install -y libvulkan-dev spirv-tools
                RESULT_VARIABLE VULKAN_INSTALL_RESULT
                OUTPUT_QUIET ERROR_QUIET
            )
            if(VULKAN_INSTALL_RESULT EQUAL 0)
                message(STATUS "Vulkan installed successfully")
            else()
                message(WARNING "Failed to install Vulkan. Please run: sudo apt install -y libvulkan-dev spirv-tools")
            endif()
        endif()
        
        # Check SDL3 build dependencies (FreeType, harfbuzz, pkg-config)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(FREETYPE freetype2 QUIET)
            pkg_check_modules(HARFBUZZ harfbuzz QUIET)
        endif()
        
        if(NOT PkgConfig_FOUND OR NOT FREETYPE_FOUND OR NOT HARFBUZZ_FOUND)
            message(STATUS "SDL3 build dependencies not found - attempting to install...")
            execute_process(
                COMMAND sudo apt install -y libfreetype6-dev libharfbuzz-dev pkg-config
                RESULT_VARIABLE SDL_DEPS_RESULT
                OUTPUT_QUIET ERROR_QUIET
            )
            if(SDL_DEPS_RESULT EQUAL 0)
                message(STATUS "SDL3 dependencies installed successfully")
            else()
                message(WARNING "Failed to install SDL3 dependencies. Please run: sudo apt install -y libfreetype6-dev libharfbuzz-dev pkg-config")
            endif()
        endif()
        
    elseif(APPLE)
        # macOS dependencies (Metal is built-in, no Vulkan needed)
        message(STATUS "macOS detected - using built-in Metal framework")
        
    elseif(WIN32)
        # Windows dependencies
        message(STATUS "Windows detected - checking Vulkan SDK...")
        find_package(Vulkan QUIET)
        if(NOT Vulkan_FOUND)
            message(WARNING "Vulkan SDK not found. Please install from: https://vulkan.lunarg.com/")
        else()
            message(STATUS "Vulkan SDK found")
        endif()
        
    else()
        message(STATUS "Unknown platform - skipping dependency checks")
    endif()
endfunction()