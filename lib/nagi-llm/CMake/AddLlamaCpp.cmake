# llama.cpp configuration for NAGI-LLM library
# This must be included BEFORE adding the nagi-llm subdirectory

include(ExternalProject)

if(NAGI_LLM_ENABLE_LLAMACPP)
    set(LLAMA_PREFIX ${CMAKE_BINARY_DIR}/_deps/llama)

    # Performance optimization flags
    # Note: Using -fno-finite-math-only instead of -ffast-math because llama.cpp requires non-finite math
    set(LLAMA_PERF_FLAGS "-O3 -march=native -fno-finite-math-only -funroll-loops")
    
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
        # Options: LLAMA3 (3B), LLAMA3_8B (8B), GEMMA3 (4B), PHI3 (4B), QWEN2 (7B)
        set(MODEL_NAME "LLAMA3" CACHE STRING "Select LLM model")
    endif()

    if(MODEL_NAME STREQUAL "PHI3")
        set(MODEL_URL "https://huggingface.co/bartowski/Phi-3-mini-4k-instruct-GGUF/resolve/main/Phi-3-mini-4k-instruct-Q4_K_M.gguf")
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
            START_OF_SYSTEM="<start_of_turn>user\\n"
            END_OF_SYSTEM="<end_of_turn>\\n"
            START_OF_USER="<start_of_turn>user\\n"
            END_OF_USER="<end_of_turn>\\n"
            START_OF_ASSISTANT="<start_of_turn>model\\n"
            END_OF_ASSISTANT="<end_of_turn>\\n")
    elseif(MODEL_NAME STREQUAL "LLAMA3")
        set(MODEL_URL "https://huggingface.co/hugging-quants/Llama-3.2-3B-Instruct-Q4_K_M-GGUF/resolve/main/llama-3.2-3b-instruct-q4_k_m.gguf")
        message(STATUS "Configuring for Llama 3.2 3B (2.3GB)")
        target_compile_definitions(nagi-llm PRIVATE
            START_OF_SYSTEM="<|begin_of_text|><|start_header_id|>system<|end_header_id|>\\n"
            END_OF_SYSTEM="<|eot_id|>"
            START_OF_USER="<|start_header_id|>user<|end_header_id|>\\n"
            END_OF_USER="<|eot_id|>"            
            START_OF_ASSISTANT="<|start_header_id|>assistant<|end_header_id|>\\n"
            END_OF_ASSISTANT="<|eot_id|>")
    elseif(MODEL_NAME STREQUAL "LLAMA3_8B")
        set(MODEL_URL "https://huggingface.co/bartowski/Meta-Llama-3.1-8B-Instruct-GGUF/resolve/main/Meta-Llama-3.1-8B-Instruct-Q4_K_M.gguf")
        message(STATUS "Configuring for Llama 3.1 8B (4.9GB) - BETTER multilingual")
        target_compile_definitions(nagi-llm PRIVATE
            START_OF_SYSTEM="<|begin_of_text|><|start_header_id|>system<|end_header_id|>\\n"
            END_OF_SYSTEM="<|eot_id|>"
            START_OF_USER="<|start_header_id|>user<|end_header_id|>\\n"
            END_OF_USER="<|eot_id|>"            
            START_OF_ASSISTANT="<|start_header_id|>assistant<|end_header_id|>\\n"
            END_OF_ASSISTANT="<|eot_id|>")
    elseif(MODEL_NAME STREQUAL "QWEN2")
        set(MODEL_URL "https://huggingface.co/WSDW/Qwen2.5-7B-Instruct-Q4_K_M-GGUF/resolve/main/qwen2.5-7b-instruct-q4_k_m.gguf")
        # set(MODEL_URL "https://huggingface.co/ldostadi/Qwen3-8B-abliterated-Q4_K_M-GGUF/resolve/main/qwen3-8b-abliterated-q4_k_m.gguf")
        message(STATUS "Configuring for Qwen2.5 7B (4.8GB) - EXCELLENT multilingual")
        target_compile_definitions(nagi-llm PRIVATE
            START_OF_SYSTEM="<|im_start|>system\\n"
            END_OF_SYSTEM="<|im_end|>\\n"
            START_OF_USER="<|im_start|>user\\n"
            END_OF_USER="<|im_end|>\\n"
            START_OF_ASSISTANT="<|im_start|>assistant\\n"
            END_OF_ASSISTANT="<|im_end|>\\n")
    else()
        message(FATAL_ERROR "Unknown MODEL_NAME: ${MODEL_NAME}. Options: LLAMA3, LLAMA3_8B, QWEN2, GEMMA3, PHI3")
    endif()

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

    # Export variables to parent scope
    set(LLAMA_SOURCE_DIR ${LLAMA_SOURCE_DIR} PARENT_SCOPE)
    set(LLAMA_BUILD_DIR ${LLAMA_BUILD_DIR} PARENT_SCOPE)
    set(LLAMA_INCLUDE_DIR ${LLAMA_INCLUDE_DIR} PARENT_SCOPE)
    set(NAGI_LL_LIBS ${NAGI_LL_LIBS} PARENT_SCOPE)
    set(MODEL_URL ${MODEL_URL} PARENT_SCOPE)

    message(STATUS "LLM (llama.cpp) configuration prepared")
    message(STATUS "Llama.cpp source dir: ${LLAMA_SOURCE_DIR}")
    message(STATUS "Llama.cpp include dir: ${LLAMA_INCLUDE_DIR}")
    message(STATUS "Model URL: ${MODEL_URL}")
endif()
