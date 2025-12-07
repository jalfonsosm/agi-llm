# llama.cpp configuration for NAGI-LLM library
# This must be included BEFORE adding the nagi-llm subdirectory

include(ExternalProject)

if(NAGI_LLM_ENABLE_LLAMACPP)
    set(LLAMA_PREFIX ${CMAKE_BINARY_DIR}/_deps/llama)

    # Performance optimization flags
    set(LLAMA_PERF_FLAGS "-O3 -march=native -ffast-math -funroll-loops")
    
    set(LLAMA_CMAKE_FLAGS
        -DLLAMA_BUILD_SHARED=OFF
        -DLLAMA_STATIC=ON
        -DBUILD_SHARED_LIBS=OFF
        -DLLAMA_ALL_WARNINGS=OFF
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        # Enable native CPU optimizations (auto-detect AVX/AVX2/AVX512)
        -DLLAMA_NATIVE=ON
        -DLLAMA_ACCELERATE=ON
        -DLLAMA_FLASH_ATTN=ON
        # Performance compiler flags
        -DCMAKE_C_FLAGS_RELEASE=${LLAMA_PERF_FLAGS}
        -DCMAKE_CXX_FLAGS_RELEASE=${LLAMA_PERF_FLAGS}
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

        list(APPEND LLAMA_CMAKE_FLAGS
            -DLLAMA_METAL=ON
            -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
            -DCMAKE_OSX_ARCHITECTURES=arm64
        )
    endif()
        
    if(NOT DEFINED MODEL_NAME)
        set(MODEL_NAME "GEMMA3" CACHE STRING "Select the LLM model: PHI3, GEMMA3)")
    endif()

    if(MODEL_NAME STREQUAL "PHI3")
        set(MODEL_URL "https://huggingface.co/bartowski/Phi-3-mini-4k-instruct-GGUF/resolve/main/Phi-3-mini-4k-instruct-Q4_K_M.gguf")
        # set(MODEL_URL "https://huggingface.co/bartowski/Phi-3-mini-4k-instruct-GGUF/blob/main/Phi-3-mini-4k-instruct-Q3_K_L.gguf")
        message(STATUS "Configuring for Phi-3 prompts.")
        target_compile_definitions(nagi-llm PRIVATE
            START_OF_SYSTEM="<|system|>\\n"
            END_OF_SYSTEM="<|end|>\\n"
            START_OF_USER="<|user|>\\n"
            END_OF_USER="<|end|>\\n"
            START_OF_ASSISTANT="<|assistant|>\\n"
            END_OF_ASSISTANT="<|end|>\\n"
        )
    elseif(MODEL_NAME STREQUAL "GEMMA3")
        set(MODEL_URL "https://huggingface.co/Aldaris/gemma-3-4b-it-Q4_K_M-GGUF/resolve/main/gemma-3-4b-it-q4_k_m.gguf")
        message(STATUS "Configuring for Gemma 3 prompts.")
        target_compile_definitions(nagi-llm PRIVATE
            START_OF_SYSTEM="<start_of_system>\\n"
            END_OF_SYSTEM="<end_of_system>\\n"
            START_OF_USER="<start_of_user>\\n"
            END_OF_USER="<end_of_user>\\n"
            START_OF_ASSISTANT="<start_of_assistant>\\n"
            END_OF_ASSISTANT="<end_of_assistant>\\n"
        )
    else()
        message(FATAL_ERROR "Unknown MODEL_NAME: ${MODEL_NAME}. Use PHI3 or GEMMA3.")
    endif()

    set(DEFAULT_MODEL_PATH "${CMAKE_BINARY_DIR}/models/llamacpp_model.gguf")

    ExternalProject_Add(llama_cpp
        GIT_REPOSITORY "https://github.com/ggerganov/llama.cpp.git"
        GIT_TAG master
        PREFIX ${LLAMA_PREFIX}
        UPDATE_COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR> git submodule update --init --recursive
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -S <SOURCE_DIR> -B <BINARY_DIR> ${LLAMA_CMAKE_FLAGS}
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR>
        INSTALL_COMMAND ""
    )

    # Export paths for use by nagi-llm and nagi
    set(LLAMA_SOURCE_DIR ${LLAMA_PREFIX}/src/llama_cpp)
    set(LLAMA_BUILD_DIR ${LLAMA_PREFIX}/src/llama_cpp-build)
    set(LLAMA_INCLUDE_DIR ${LLAMA_PREFIX}/src/llama_cpp/include)

    set(LLAMA_LIB ${LLAMA_BUILD_DIR}/src/libllama.a)
    set(GGML_LIB ${LLAMA_BUILD_DIR}/ggml/src/libggml.a)
    set(GGML_BASE_LIB ${LLAMA_BUILD_DIR}/ggml/src/libggml-base.a)

    set(GGML_BACKEND_LIBS
        ${LLAMA_BUILD_DIR}/ggml/src/ggml-blas/libggml-blas.a
        ${LLAMA_BUILD_DIR}/ggml/src/libggml-cpu.a
        ${LLAMA_BUILD_DIR}/ggml/src/ggml-metal/libggml-metal.a
    )

    set(NAGI_LL_LIBS
        ${LLAMA_LIB}
        ${GGML_LIB}
        ${GGML_BASE_LIB}
    )

    list(APPEND NAGI_LL_LIBS ${GGML_BACKEND_LIBS})

    # Add Apple frameworks to llama.cpp libraries
    if(APPLE)
        list(APPEND NAGI_LL_LIBS "-framework Accelerate")
        list(APPEND NAGI_LL_LIBS "-framework Metal" "-framework MetalPerformanceShaders")
    endif()

    # NAGI_LL_LIBS is now available for use by nagi-llm

    message(STATUS "LLM (llama.cpp) configuration prepared")
    message(STATUS "Llama.cpp source dir: ${LLAMA_SOURCE_DIR}")
    message(STATUS "Llama.cpp include dir: ${LLAMA_INCLUDE_DIR}")
endif()
