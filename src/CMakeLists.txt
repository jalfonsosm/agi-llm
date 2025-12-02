add_executable(nagi)

set_target_properties(nagi PROPERTIES
    C_STANDARD 11
    C_EXTENSIONS NO
)

option(NAGI_ENABLE_LLM "Enable LLM (llama.cpp) support" ON)

# All include files references are relative to the source file (not ideal)
# target_include_directories(nagi PRIVATE .)

set(base_sources
    agi.c
    agi.h
    agi_string.c
    agi_string.h
    base.c
    base.h
    cmd_misc.c
    cmd_misc.h
    config.c
    config.h
    debug.c
    debug.h
    decrypt.c
    decrypt.h
    flags.c
    flags.h
    game_id.c
    game_id.h
    initialise.c
    initialise.h
    list.c
    list.h
    log.c
    log.h
    nagi_main.c
    new_room.c
    new_room.h
    obj_show.c
    obj_show.h
    objects.c
    objects.h
    resource.h
    state_info.c
    state_info.h
    state_io.c
    state_io.h
    trace.c
    trace.h
)

set(logic_sources
    logic/arithmetic.c
    logic/arithmetic.h
    logic/cmd_table.c
    logic/cmd_table.h
    logic/logic_base.c
    logic/logic_base.h
    logic/logic_eval.c
    logic/logic_eval.h
    logic/logic_execute.c
    logic/logic_execute.h
    logic/llm.c
    logic/llm.h
)

set(picture_sources
    picture/pic_add.c
    picture/pic_add.h
    picture/pic_render.c
    picture/pic_render.h
    picture/pic_res.c
    picture/pic_res.h
    picture/sbuf_util.c
    picture/sbuf_util.h
)

set(res_sources
    res/res.h
    res/res_dir.c
    res/res_lzw.c
    res/res_pic.c
    res/res_vol.c
)

set(sound_sources
    sound/pcm_out.c
    sound/pcm_out.h
    sound/pcm_out_sdl.c
    sound/pcm_out_sdl.h
    sound/sound.h
    sound/sound_base.c
    sound/sound_base.h
    sound/sound_gen.c
    sound/sound_gen.h
    sound/tone.c
    sound/tone.h
    sound/tone_pcm.c
    sound/tone_pcm.h
)

set(video_sources
    sys/chargen.c
    sys/chargen.h
    sys/drv_video.h
    sys/gfx.c
    sys/gfx.h
    sys/sdl_vid.c
    sys/sdl_vid.h
    sys/vid_render.c
    sys/vid_render.h
)

set(sys_sources
    sys/agi_file.c
    sys/agi_file.h
    sys/delay.c
    sys/delay.h
    sys/drvpick.c
    sys/drvpick.h
    sys/endian.c
    sys/endian.h
    sys/error.c
    sys/error.h
    sys/glob_sys.c
    sys/glob_sys.h
    sys/ini_config.c
    sys/ini_config.h
    sys/mem_wrap.c
    sys/mem_wrap.h
    sys/memory.c
    sys/memory.h
    sys/rand.c
    sys/rand.h
    sys/script.c
    sys/script.h
    sys/sys_dir.c
    sys/sys_dir.h
    sys/time.c
    sys/time.h
    sys/vstring.c
    sys/vstring.h
)

set(ui_sources
    ui/agi_text.c
    ui/agi_text.h
    ui/cmd_input.c
    ui/cmd_input.h
    ui/controller.c
    ui/controller.h
    ui/events.c
    ui/events.h
    ui/list_box.c
    ui/list_box.h
    ui/menu.c
    ui/menu.h
    ui/mouse.c
    ui/mouse.h
    ui/msg.c
    ui/msg.h
    ui/parse.c
    ui/parse.h
    ui/printf.c
    ui/printf.h
    ui/status.c
    ui/status.h
    ui/string.c
    ui/string.h
    ui/window.c
    ui/window.h
)

set(version_sources
    version/agi_crc.c
    version/agi_crc.h
    version/standard.c
    version/standard.h
)

set(view_sources
    view/obj_base.c
    view/obj_base.h
    view/obj_blit.c
    view/obj_blit.h
    view/obj_block.c
    view/obj_block.h
    view/obj_cycle.c
    view/obj_cycle.h
    view/obj_drawerase.c
    view/obj_drawerase.h
    view/obj_loop.c
    view/obj_loop.h
    view/obj_motion.c
    view/obj_motion.h
    view/obj_motion_cmd.c
    view/obj_motion_cmd.h
    view/obj_picbuff.c
    view/obj_picbuff.h
    view/obj_position.c
    view/obj_position.h
    view/obj_priority.c
    view/obj_priority.h
    view/obj_proximity.c
    view/obj_proximity.h
    view/obj_update.c
    view/obj_update.h
    view/view_base.c
    view/view_base.h
)

set(lib_sources
    lib/utf8_decode.c
    lib/utf8_decode.h
)

if(NAGI_ENABLE_LLM)
    set(llm_sources
        llm/llm_context.c
        llm/llm_context.h
        llm/llm_parser.c
        llm/llm_parser.h
    )
endif()

target_sources(nagi
    PRIVATE
    ${base_sources}
    ${logic_sources}
    ${picture_sources}
    ${res_sources}
    ${sound_sources}
    ${sys_sources}
    ${ui_sources}
    ${version_sources}
    ${video_sources}
    ${view_sources}
    ${lib_sources}
)

if(NAGI_ENABLE_LLM)
    target_sources(nagi PRIVATE ${llm_sources})
endif()

target_link_libraries(nagi PRIVATE ${SDL3_TARGET})

include(ExternalProject)
if(NAGI_ENABLE_LLM)
    set(LLAMA_PREFIX ${CMAKE_BINARY_DIR}/_deps/llama)

    set(LLAMA_CMAKE_FLAGS
        -DLLAMA_BUILD_SHARED=OFF
        -DLLAMA_STATIC=ON
        -DBUILD_SHARED_LIBS=OFF
        -DLLAMA_ALL_WARNINGS=OFF
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DLLAMA_NATIVE=OFF
        -DLLAMA_AVX=OFF
        -DLLAMA_AVX2=OFF
        -DLLAMA_AVX512=OFF
        -DLLAMA_FMA=OFF
        -DLLAMA_F16C=OFF
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

    ExternalProject_Add(llama_cpp
        GIT_REPOSITORY "https://github.com/ggerganov/llama.cpp.git"
        GIT_TAG master
        PREFIX ${LLAMA_PREFIX}
        UPDATE_COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR> git submodule update --init --recursive
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -S <SOURCE_DIR> -B <BINARY_DIR> ${LLAMA_CMAKE_FLAGS}
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR>
        INSTALL_COMMAND ""
    )
    
    set(LLAMA_SOURCE_DIR ${LLAMA_PREFIX}/src/llama_cpp)
    set(LLAMA_BUILD_DIR ${LLAMA_PREFIX}/src/llama_cpp-build)
    set(LLAMA_INCLUDE_DIR ${LLAMA_SOURCE_DIR}/include)

    add_dependencies(nagi llama_cpp)
    
    target_include_directories(nagi PRIVATE 
        ${LLAMA_SOURCE_DIR}
        ${LLAMA_INCLUDE_DIR}
        ${LLAMA_SOURCE_DIR}/ggml/include
    )

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

    target_link_libraries(nagi PRIVATE ${NAGI_LL_LIBS})

    if(APPLE)
        target_link_libraries(nagi PRIVATE "-framework Accelerate")
        target_link_libraries(nagi PRIVATE "-framework Metal" "-framework MetalPerformanceShaders")
    endif()
        
    set(DEFAULT_MODEL_PATH "${CMAKE_BINARY_DIR}/models/Phi-3-mini-4k-instruct-q4.gguf")
    target_compile_definitions(nagi PRIVATE
        NAGI_ENABLE_LLM=1
        NAGI_DEFAULT_LLM_MODEL_PATH="${DEFAULT_MODEL_PATH}"
    )

    add_custom_command(
        OUTPUT "${DEFAULT_MODEL_PATH}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/models"
        COMMAND ${CMAKE_COMMAND} -E echo "Downloading Phi-3-mini model to ${DEFAULT_MODEL_PATH}..."
        COMMAND curl -f -L --retry 3 -o "${DEFAULT_MODEL_PATH}"
            "https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-gguf/resolve/main/Phi-3-mini-4k-instruct-q4.gguf"
        COMMENT "Downloading Phi-3-mini-4k-instruct (Q4, 2.3GB) model..."
        VERBATIM
    )

    add_custom_target(download_model DEPENDS "${DEFAULT_MODEL_PATH}")
    add_dependencies(nagi download_model)
    
    message(STATUS "LLM (llama.cpp) support enabled - static library")
    message(STATUS "Llama.cpp include dir: ${LLAMA_SOURCE_DIR}")
else()
    target_compile_definitions(nagi PRIVATE NAGI_NO_LLM=1)
    message(STATUS "LLM support disabled")
endif()

# Math library (needed by llama.cpp on Unix)
if(UNIX)
    target_link_libraries(nagi PRIVATE m dl pthread)
endif()

if(APPLE)
    target_link_libraries(nagi PRIVATE c++)
else()
    target_link_libraries(nagi PRIVATE stdc++)
endif()

# Compile definitions (needed for strdup, strcasecmp on some systems)
target_compile_definitions(nagi PRIVATE _DEFAULT_SOURCE)

# Compiler warning flags - applied only to nagi, not to SDL3
target_compile_options(nagi PRIVATE
    # Basic flags
    -fsigned-char
    -fno-strict-aliasing
    -fwrapv

    # Warnings (common to GCC and Clang)
    -Wall
    -Wextra
    -Wcast-align
    -Wcast-qual
    -Wconversion
    -Wdouble-promotion
    -Wformat=2
    -Wmissing-declarations
    -Wmissing-prototypes
    -Wnull-dereference
    -Wold-style-definition
    -Wpointer-arith
    -Wshadow
    -Wshift-negative-value
    -Wstrict-prototypes
    -Wswitch-default
    -Wundef
    -Wuninitialized
    -Wwrite-strings

    # Errors for specific issues
    -Werror=implicit-function-declaration
    -Werror=return-type
    -Werror=unused-result
)

# GCC-specific warnings
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(nagi PRIVATE
        -Wduplicated-branches
        -Wduplicated-cond
        -Wformat-overflow=2
        -Wformat-signedness
        -Wformat-truncation=2
        -Winit-self
        -Wlogical-op
        -Wshift-overflow=2
        -Wstrict-overflow=2
    )
endif()

# Clang-specific warnings
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(nagi PRIVATE
        -Wconditional-uninitialized
        -Wmissing-variable-declarations
        -Wunreachable-code-break
        -Wunreachable-code-return
    )
endif()

set_target_properties(nagi PROPERTIES
    RUNTIME_OUTPUT_NAME nagi
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

if (WIN32)
    target_sources(nagi PRIVATE nagi.rc)
    set_target_properties(nagi PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# Copy data files (fonts, config) to build directory
set(NAGI_DATA_FILES
    ${CMAKE_SOURCE_DIR}/bin/font_4x8.nbf
    ${CMAKE_SOURCE_DIR}/bin/font_8x8.nbf
    ${CMAKE_SOURCE_DIR}/bin/font_16x16.nbf
    ${CMAKE_SOURCE_DIR}/bin/nagi.ini
    ${CMAKE_SOURCE_DIR}/bin/standard.ini
)

add_custom_command(TARGET nagi POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${NAGI_DATA_FILES}
        ${CMAKE_BINARY_DIR}
    COMMENT "Copying NAGI data files to build directory"
)
