# BitNet.cpp configuration for NAGI-LLM library

include(ExternalProject)

if(NAGI_LLM_ENABLE_BITNET)
    set(BITNET_PREFIX ${CMAKE_BINARY_DIR}/_deps/bitnet)

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)")
        set(BITNET_ARM_TL1 ON)
        set(BITNET_X86_TL2 OFF)
        message(STATUS "BitNet: ARM platform detected, enabling TL1 kernels")
    else()
        set(BITNET_ARM_TL1 OFF)
        set(BITNET_X86_TL2 ON)
        message(STATUS "BitNet: x86 platform detected, enabling TL2 kernels")
    endif()

    # === APPLE SILICON FIX: Disable problematic LLVM optimization ===
    if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
        # Use -O2 for both Debug and Release to avoid LLVM bug
        set(BITNET_OPT_FLAGS "-O2 -mllvm -disable-interleaved-load-combine")
        message(STATUS "BitNet: Apple Silicon detected, applying LLVM workaround")
        message(STATUS "BitNet: Using ${BITNET_OPT_FLAGS} to prevent compilation hangs")
    else()
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(BITNET_OPT_FLAGS "-O2")
            message(STATUS "BitNet: Using -O2 optimization (faster compilation for Debug)")
        else()
            set(BITNET_OPT_FLAGS "-O3")
            message(STATUS "BitNet: Using -O3 optimization (maximum performance for Release)")
        endif()
    endif()

    # Find Homebrew LLVM 18 for Apple Silicon
    if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
        find_program(CLANG_18 clang-18
            PATHS /opt/homebrew/opt/llvm@18/bin
            NO_DEFAULT_PATH
        )
        find_program(CLANGXX_18 clang++-18
            PATHS /opt/homebrew/opt/llvm@18/bin
            NO_DEFAULT_PATH
        )
        
        if(CLANG_18 AND CLANGXX_18)
            set(BITNET_C_COMPILER "${CLANG_18}")
            set(BITNET_CXX_COMPILER "${CLANGXX_18}")
            message(STATUS "BitNet: Found Homebrew LLVM 18 for Apple Silicon")
        else()
            message(WARNING "BitNet: Homebrew LLVM 18 not found. Install with: brew install llvm@18")
            set(BITNET_C_COMPILER "clang")
            set(BITNET_CXX_COMPILER "clang++")
        endif()
    else()
        set(BITNET_C_COMPILER "clang")
        set(BITNET_CXX_COMPILER "clang++")
    endif()

    set(BITNET_CMAKE_FLAGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DBITNET_ARM_TL1=${BITNET_ARM_TL1}
        -DBITNET_X86_TL2=${BITNET_X86_TL2}
        -DGGML_BITNET_ARM_TL1=${BITNET_ARM_TL1}
        -DGGML_BITNET_X86_TL2=${BITNET_X86_TL2}
        -DGGML_ACCELERATE=ON
        -DBUILD_SHARED_LIBS=OFF
        -DCMAKE_C_COMPILER=${BITNET_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${BITNET_CXX_COMPILER}
        -DCMAKE_CXX_FLAGS_DEBUG=${BITNET_OPT_FLAGS}
        -DCMAKE_CXX_FLAGS_RELEASE=${BITNET_OPT_FLAGS}
        -DCMAKE_C_FLAGS=${BITNET_OPT_FLAGS}
    )

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)")
        list(APPEND BITNET_CMAKE_FLAGS
            -DCMAKE_OSX_ARCHITECTURES=arm64
        )
    endif()


    # Common patch to add GGUF model support to setup_env.py
    set(BITNET_COMMON_PATCH
        COMMAND ${CMAKE_COMMAND} -E echo "BitNet: Patching setup_env.py to support GGUF model download..."
        COMMAND python3 "${CMAKE_SOURCE_DIR}/CMake/patch_bitnet.py" "<SOURCE_DIR>/setup_env.py"
    )

    if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
        set(BITNET_PLATFORM_PATCH 
            # First, backup the original file
            COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/setup_env.py <SOURCE_DIR>/setup_env.py.backup
            # Apply Apple Silicon fixes
            COMMAND sed -i.bak
                -e "s/\\\"-DCMAKE_C_COMPILER=clang\\\"/\\\"-DCMAKE_C_COMPILER=${BITNET_C_COMPILER}\\\", \\\"-DCMAKE_C_FLAGS=${BITNET_OPT_FLAGS}\\\"/g"
                -e "s/\\\"-DCMAKE_CXX_COMPILER=clang..\\\"/\\\"-DCMAKE_CXX_COMPILER=${BITNET_CXX_COMPILER}\\\", \\\"-DCMAKE_CXX_FLAGS=${BITNET_OPT_FLAGS}\\\", \\\"-DCMAKE_CXX_FLAGS_RELEASE=${BITNET_OPT_FLAGS}\\\"/g"
                -e "s/, \\\"--config\\\", \\\"Release\\\"//g"
                <SOURCE_DIR>/setup_env.py
        )
        message(STATUS "BitNet: Apple Silicon patch will be applied to setup_env.py")
    elseif(CMAKE_GENERATOR STREQUAL "Xcode" OR CMAKE_GENERATOR STREQUAL "Visual Studio 17 2022")
        set(BITNET_PLATFORM_PATCH "")
        message(STATUS "BitNet: Multi-config generator detected, no platform patch needed")
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(BITNET_PLATFORM_PATCH "")
        message(STATUS "BitNet: Debug mode, no platform patch needed; using -O2 flags via BITNET_OPT_FLAGS")
    else()
        set(BITNET_PLATFORM_PATCH "")
        message(STATUS "BitNet: Release mode with Makefiles, no platform patch needed")
    endif()

    set(BITNET_PATCH_CMD ${BITNET_COMMON_PATCH} ${BITNET_PLATFORM_PATCH})

    # Select BitNet model for setup (needed to generate kernels)
    # set(BITNET_HF_MODEL "1bitLLM/bitnet_b1_58-large")
    set(BITNET_HF_MODEL "microsoft/BitNet-b1.58-2B-4T-gguf")
    # set(BITNET_HF_MODEL "tiiuae/Falcon3-1B-Instruct-1.58bit")
    # set(BITNET_HF_MODEL "1bitLLM/bitnet_b1_58-3B")
    
    # Note: setup_env.py uses the model name (without user/org) as the directory name
    set(MODEL_URL "https://huggingface.co/microsoft/BitNet-b1.58-2B-4T-gguf/resolve/main/ggml-model-i2_s.gguf")        
    message(STATUS "LLM Model: BitNet 1.58-bit (2B parameters)")

    ExternalProject_Add(bitnet_cpp
        GIT_REPOSITORY "https://github.com/microsoft/BitNet.git"
        GIT_TAG main
        GIT_SUBMODULES "3rdparty/llama.cpp"
        PREFIX ${BITNET_PREFIX}
        UPDATE_COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR> git submodule update --init --recursive
        PATCH_COMMAND ${BITNET_PATCH_CMD}
        CONFIGURE_COMMAND
            COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
            COMMAND ${CMAKE_COMMAND} -E echo "BitNet: Downloading model and generating kernels..."
            COMMAND ${CMAKE_COMMAND} -E echo "Model: ${BITNET_HF_MODEL} (~700MB)"
            COMMAND ${CMAKE_COMMAND} -E echo "This will take several minutes, please wait..."
            COMMAND ${CMAKE_COMMAND} -E echo "Apple Silicon fix applied: ${BITNET_OPT_FLAGS}"
            COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
            COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR> python3 setup_env.py --hf-repo ${BITNET_HF_MODEL}
            COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
            COMMAND ${CMAKE_COMMAND} -E echo "BitNet: Model downloaded, configuring build..."
            COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
            COMMAND ${CMAKE_COMMAND} -S <SOURCE_DIR> -B <BINARY_DIR> ${BITNET_CMAKE_FLAGS}
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target llama --target ggml
        INSTALL_COMMAND ""
    )

    # Export paths for use by nagi-llm
    set(BITNET_SOURCE_DIR ${BITNET_PREFIX}/src/bitnet_cpp)
    set(BITNET_BUILD_DIR ${BITNET_PREFIX}/src/bitnet_cpp-build)
    set(BITNET_INCLUDE_DIR ${BITNET_SOURCE_DIR}/3rdparty/llama.cpp/include)
    set(BITNET_GGML_INCLUDE_DIR ${BITNET_SOURCE_DIR}/3rdparty/llama.cpp/ggml/include)
    set(BITNET_SRC_INCLUDE_DIR ${BITNET_SOURCE_DIR}/include)

    # BitNet libraries (uses llama.cpp libs with BitNet kernels compiled in)
    set(BITNET_LLAMA_LIB ${BITNET_BUILD_DIR}/3rdparty/llama.cpp/src/libllama.a)
    set(BITNET_GGML_LIB ${BITNET_BUILD_DIR}/3rdparty/llama.cpp/ggml/src/libggml.a)

    set(NAGI_BITNET_LIBS
        ${BITNET_LLAMA_LIB}
        ${BITNET_GGML_LIB}
    )

    # Add system frameworks on macOS
    if(APPLE)
        list(APPEND NAGI_BITNET_LIBS "-framework Accelerate")
    endif()

    message(STATUS "BitNet source dir: ${BITNET_SOURCE_DIR}")
    message(STATUS "BitNet include dir: ${BITNET_INCLUDE_DIR}")

    set(BITNET_MODEL_PATH "${BITNET_SOURCE_DIR}/models/BitNet-b1.58-2B-4T/ggml-model-i2_s.gguf" CACHE INTERNAL "Path to BitNet model")
endif()
