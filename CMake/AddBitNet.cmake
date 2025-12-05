# BitNet.cpp configuration for NAGI-LLM library
# BitNet is a 1.58-bit LLM framework built on top of llama.cpp with optimized kernels

include(ExternalProject)


if(NAGI_LLM_ENABLE_BITNET)
    set(BITNET_PREFIX ${CMAKE_BINARY_DIR}/_deps/bitnet)

    # Detect ARM vs x86 for kernel selection
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)")
        set(BITNET_ARM_TL1 ON)
        set(BITNET_X86_TL2 OFF)
        message(STATUS "BitNet: ARM platform detected, enabling TL1 kernels")
    else()
        set(BITNET_ARM_TL1 OFF)
        set(BITNET_X86_TL2 ON)
        message(STATUS "BitNet: x86 platform detected, enabling TL2 kernels")
    endif()

    # Adjust optimization level based on build type
    # Debug: Use -O2 for faster compilation (~15 min instead of 2+ hours)
    # Release: Use -O3 for maximum runtime performance
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(BITNET_OPT_FLAGS "-O2")
        message(STATUS "BitNet: Using -O2 optimization (faster compilation for Debug)")
    else()
        set(BITNET_OPT_FLAGS "-O3")
        message(STATUS "BitNet: Using -O3 optimization (maximum performance for Release)")
    endif()

    set(BITNET_CMAKE_FLAGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DBITNET_ARM_TL1=${BITNET_ARM_TL1}
        -DBITNET_X86_TL2=${BITNET_X86_TL2}
        -DGGML_BITNET_ARM_TL1=${BITNET_ARM_TL1}
        -DGGML_BITNET_X86_TL2=${BITNET_X86_TL2}
        -DBUILD_SHARED_LIBS=OFF
        -DCMAKE_CXX_FLAGS_DEBUG=${BITNET_OPT_FLAGS}
        -DCMAKE_CXX_FLAGS_RELEASE=-O3
    )

    if(APPLE)
        if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
            execute_process(COMMAND sw_vers -productVersion
                OUTPUT_VARIABLE _swvers OUTPUT_STRIP_TRAILING_WHITESPACE)
            string(REGEX MATCH "^[0-9]+\.[0-9]+" _mac_ver "${_swvers}")
            if(_mac_ver)
                set(CMAKE_OSX_DEPLOYMENT_TARGET "${_mac_ver}" CACHE STRING "macOS deployment target" FORCE)
            endif()
        endif()

        if(CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS "10.15")
            set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "macOS deployment target" FORCE)
        endif()

        list(APPEND BITNET_CMAKE_FLAGS
            -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
            -DCMAKE_OSX_ARCHITECTURES=arm64
            # Ensure Accelerate is used for BLAS on macOS
            -DGGML_ACCELERATE=ON
        )
    endif()

    # Select BitNet model for setup (needed to generate kernels)
    # set(BITNET_HF_MODEL "1bitLLM/bitnet_b1_58-large")
    set(BITNET_HF_MODEL "microsoft/BitNet-b1.58-2B-4T")
    # set(BITNET_HF_MODEL "tiiuae/Falcon3-1B-Instruct-1.58bit")
    # set(BITNET_HF_MODEL "1bitLLM/bitnet_b1_58-3B")

    # Determine if we need to patch setup_env.py
    # Xcode (multi-config): No patch needed, --config Release works
    # Unix Makefiles in Debug: Need patch, --config not supported
    # Unix Makefiles in Release: No patch needed (matches the hardcoded Release)
    if(CMAKE_GENERATOR STREQUAL "Xcode" OR CMAKE_GENERATOR STREQUAL "Visual Studio 17 2022")
        # Multi-config generators support --config flag
        set(BITNET_PATCH_CMD "")
        message(STATUS "BitNet: Multi-config generator detected, no patch needed")
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
        # Single-config generator in Debug mode needs patch
        set(BITNET_PATCH_CMD sed -i.bak 
            -e "s/, \\\"--config\\\", \\\"Release\\\"//g" 
            -e "s/\\\"-DCMAKE_CXX_COMPILER=clang\\+\\+\\\"/\\\"-DCMAKE_CXX_COMPILER=clang\\+\\+\\\", \\\"-DCMAKE_BUILD_TYPE=Debug\\\", \\\"-DCMAKE_CXX_FLAGS=-O2\\\"/g"
            <SOURCE_DIR>/setup_env.py)
        message(STATUS "BitNet: Debug mode, patching setup_env.py to use -O2 and remove --config")
    else()
        # Single-config generator in Release mode, no patch needed (matches hardcoded Release)
        set(BITNET_PATCH_CMD "")
        message(STATUS "BitNet: Release mode with Makefiles, no patch needed")
    endif()

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
    set(BITNET_GGML_BASE_LIB ${BITNET_BUILD_DIR}/3rdparty/llama.cpp/ggml/src/libggml-base.a)
    set(BITNET_GGML_CPU_LIB ${BITNET_BUILD_DIR}/3rdparty/llama.cpp/ggml/src/libggml-cpu.a)

    # BitNet-specific libraries
    set(BITNET_LUT_LIB ${BITNET_BUILD_DIR}/src/libggml-bitnet-lut.a)
    set(BITNET_MAD_LIB ${BITNET_BUILD_DIR}/src/libggml-bitnet-mad.a)

    set(NAGI_BITNET_LIBS
        ${BITNET_LLAMA_LIB}
        ${BITNET_LUT_LIB}
        ${BITNET_MAD_LIB}
        ${BITNET_GGML_LIB}
        ${BITNET_GGML_BASE_LIB}
        ${BITNET_GGML_CPU_LIB}
    )

    # Add system frameworks on macOS
    if(APPLE)
        list(APPEND NAGI_BITNET_LIBS "-framework Accelerate")
    endif()

    message(STATUS "BitNet.cpp configuration prepared")
    message(STATUS "BitNet source dir: ${BITNET_SOURCE_DIR}")
    message(STATUS "BitNet include dir: ${BITNET_INCLUDE_DIR}")
endif()
