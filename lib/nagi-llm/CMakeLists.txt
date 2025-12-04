# NAGI-LLM Library - Abstract LLM interface for NAGI AGI engine
cmake_minimum_required(VERSION 3.15)
project(nagi-llm VERSION 1.0.0 LANGUAGES C)

# Library options
option(NAGI_LLM_ENABLE_LLAMACPP "Enable llama.cpp backend" ON)
option(NAGI_LLM_ENABLE_BITNET "Enable BitNet backend" OFF)
option(NAGI_LLM_ENABLE_CLOUD_API "Enable cloud API backend" OFF)

# Create library
add_library(nagi-llm STATIC)

set_target_properties(nagi-llm PROPERTIES
    C_STANDARD 11
    C_EXTENSIONS NO
)

# Public headers
target_include_directories(nagi-llm PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Core sources (always included)
target_sources(nagi-llm PRIVATE
    src/nagi_llm.c
    src/nagi_llm_context.c
)

# llama.cpp backend
if(NAGI_LLM_ENABLE_LLAMACPP)
    target_sources(nagi-llm PRIVATE
        backends/llamacpp/nagi_llm_llamacpp.c
    )

    # Add backends directory to include path
    target_include_directories(nagi-llm PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/backends/llamacpp
    )

    target_compile_definitions(nagi-llm PUBLIC NAGI_LLM_HAS_LLAMACPP=1)

    # Include llama.cpp headers (provided by parent project)
    if(DEFINED LLAMA_INCLUDE_DIR)
        target_include_directories(nagi-llm PRIVATE
            ${LLAMA_SOURCE_DIR}
            ${LLAMA_INCLUDE_DIR}
            ${LLAMA_SOURCE_DIR}/ggml/include
        )
    endif()

    # Link llama.cpp libraries (provided by parent project)
    if(DEFINED NAGI_LL_LIBS)
        target_link_libraries(nagi-llm PUBLIC ${NAGI_LL_LIBS})
    endif()

    # Add dependency on llama_cpp external project
    if(TARGET llama_cpp)
        add_dependencies(nagi-llm llama_cpp)
    endif()

    message(STATUS "NAGI-LLM: llama.cpp backend enabled")
endif()

# BitNet backend (future)
if(NAGI_LLM_ENABLE_BITNET)
    target_sources(nagi-llm PRIVATE
        src/nagi_llm_bitnet.c
    )
    target_compile_definitions(nagi-llm PUBLIC NAGI_LLM_HAS_BITNET=1)
    message(STATUS "NAGI-LLM: BitNet backend enabled")
endif()

# Cloud API backend (future)
if(NAGI_LLM_ENABLE_CLOUD_API)
    target_sources(nagi-llm PRIVATE
        src/nagi_llm_cloud_api.c
    )
    target_compile_definitions(nagi-llm PUBLIC NAGI_LLM_HAS_CLOUD_API=1)
    message(STATUS "NAGI-LLM: Cloud API backend enabled")
endif()

# Common compile definitions
target_compile_definitions(nagi-llm PRIVATE
    _DEFAULT_SOURCE
)

# Warning flags
target_compile_options(nagi-llm PRIVATE
    -Wall
    -Wextra
    -Wshadow
    -Werror=implicit-function-declaration
    -Werror=return-type
)

# Installation rules (optional)
install(TARGETS nagi-llm
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)
