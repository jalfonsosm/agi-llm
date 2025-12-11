add_executable(nagi)

set_target_properties(nagi PROPERTIES
    C_STANDARD 11
    C_EXTENSIONS NO
)


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

target_link_libraries(nagi PRIVATE ${SDL3_TARGET} ${SDL3_TTF_TARGET})

if(NAGI_LLM_ENABLE_LLAMACPP OR NAGI_LLM_ENABLE_BITNET OR NAGI_LLM_ENABLE_CLOUD_API)
    target_link_libraries(nagi PRIVATE nagi-llm)
    target_include_directories(nagi PRIVATE ${CMAKE_SOURCE_DIR}/lib/nagi-llm/include)

    set(DEFAULT_MODEL_PATH "${CMAKE_BINARY_DIR}/models/llm_model.gguf")
    
    if(NAGI_LLM_ENABLE_BITNET)
        if(DEFINED BITNET_MODEL_PATH)
            add_custom_command(
                OUTPUT "${DEFAULT_MODEL_PATH}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/models"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${BITNET_MODEL_PATH}" "${DEFAULT_MODEL_PATH}"
                DEPENDS bitnet_cpp
                COMMENT "Copying BitNet model to build directory..."
                VERBATIM
            )
            
            add_custom_target(copy_bitnet_model DEPENDS "${DEFAULT_MODEL_PATH}")
            add_dependencies(nagi copy_bitnet_model)
            
            message(STATUS "LLM Model: BitNet 1.58-bit (2B parameters) will be copied to ${DEFAULT_MODEL_PATH}")
        else()
            message(WARNING "BITNET_MODEL_PATH not defined, model copy will be skipped")
        endif()
    elseif(NAGI_LLM_ENABLE_LLAMACPP)
        if(DEFINED MODEL_URL)
            add_custom_command(
                OUTPUT "${DEFAULT_MODEL_PATH}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/models"
                COMMAND ${CMAKE_COMMAND} -E echo "Downloading LLM model to ${DEFAULT_MODEL_PATH}..."
                COMMAND curl -f -L --retry 3 --ssl-no-revoke -o "${DEFAULT_MODEL_PATH}" "${MODEL_URL}"
                DEPENDS llama_cpp
                COMMENT "Downloading LLM model..."
                VERBATIM
            )

            add_custom_target(download_model DEPENDS "${DEFAULT_MODEL_PATH}")
            add_dependencies(nagi download_model)
            
            message(STATUS "LLM Model will be downloaded to ${DEFAULT_MODEL_PATH}")
        else()
            message(WARNING "MODEL_URL not defined, model download will be skipped")
        endif()
    endif()
    
    target_compile_definitions(nagi PRIVATE NAGI_ENABLE_LLM=1)

    if(NAGI_LLM_ENABLE_LLAMACPP OR NAGI_LLM_ENABLE_BITNET)
        target_compile_definitions(nagi PRIVATE NAGI_DEFAULT_LLM_MODEL_PATH="${DEFAULT_MODEL_PATH}")
    endif()

    message(STATUS "LLM support enabled")
    if(DEFINED LLAMA_SOURCE_DIR)
        message(STATUS "Llama.cpp include dir: ${LLAMA_SOURCE_DIR}")
    endif()
else()
    target_compile_definitions(nagi PRIVATE NAGI_NO_LLM=1)
    message(STATUS "LLM support disabled")
endif()


if((NAGI_LLM_ENABLE_LLAMACPP OR NAGI_LLM_ENABLE_BITNET) AND UNIX)
    target_link_libraries(nagi PRIVATE m dl pthread)
endif()

if(APPLE)
    target_link_libraries(nagi PRIVATE c++)
elseif(NOT MSVC)
    target_link_libraries(nagi PRIVATE stdc++)
endif()

# Compile definitions (needed for strdup, strcasecmp on some systems)
target_compile_definitions(nagi PRIVATE _DEFAULT_SOURCE)

# Compiler warning flags
if(MSVC)
    target_compile_options(nagi PRIVATE /W3)
else()
    target_compile_options(nagi PRIVATE
        -fsigned-char
        -fno-strict-aliasing
        -fwrapv
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
        -Werror=implicit-function-declaration
        -Werror=return-type
        -Werror=unused-result
    )
    
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
    
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(nagi PRIVATE
            -Wconditional-uninitialized
            -Wmissing-variable-declarations
            -Wunreachable-code-break
            -Wunreachable-code-return
        )
    endif()
endif()

set_target_properties(nagi PROPERTIES
    RUNTIME_OUTPUT_NAME nagi
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

if (WIN32)
    target_sources(nagi PRIVATE nagi.rc)
    # Don't set WIN32_EXECUTABLE - SDL3 handles the entry point
    # set_target_properties(nagi PROPERTIES
    #     WIN32_EXECUTABLE TRUE
    # )
endif()

# Copy data files (fonts, config) to build directory
set(NAGI_DATA_FILES
    ${CMAKE_SOURCE_DIR}/bin/font_4x8.nbf
    ${CMAKE_SOURCE_DIR}/bin/font_8x8.nbf
    ${CMAKE_SOURCE_DIR}/bin/font_16x16.nbf
    ${CMAKE_SOURCE_DIR}/bin/IBMPlexMono-Regular.ttf
    ${CMAKE_SOURCE_DIR}/bin/nagi.ini
    ${CMAKE_SOURCE_DIR}/bin/standard.ini
)

add_custom_command(TARGET nagi POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${NAGI_DATA_FILES}
        ${CMAKE_BINARY_DIR}
    COMMENT "Copying NAGI data files to build directory"
)

if(NAGI_LLM_ENABLE_CLOUD_API)
    add_custom_command(TARGET nagi POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/cloud_config.ini
            ${CMAKE_BINARY_DIR}/cloud_config.ini
        COMMENT "Copying cloud_config.ini to build directory"
    )
endif()
