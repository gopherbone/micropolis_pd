cmake_minimum_required(VERSION 3.14)
set(CMAKE_C_STANDARD 99)

set(ENVSDK $ENV{PLAYDATE_SDK_PATH})

if (NOT ${ENVSDK} STREQUAL "")
    file(TO_CMAKE_PATH ${ENVSDK} SDK)
else()
    execute_process(
        COMMAND bash -c "egrep '^\\s*SDKRoot' $HOME/.Playdate/config"
        COMMAND head -n 1
        COMMAND cut -c9-
        OUTPUT_VARIABLE SDK
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

if (NOT EXISTS ${SDK})
    message(WARNING "Playdate SDK Path not found; set PLAYDATE_SDK_PATH environment variable or $HOME/.Playdate/config")
else()
    if ($ENV{BUILD_DEVICE})
        include(${SDK}/C_API/buildsupport/arm.cmake)
    endif()
endif()

set(PLAYDATE_GAME_NAME micropolis)
set(PLAYDATE_GAME_DEVICE micropolis_DEVICE)

project(${PLAYDATE_GAME_NAME} C ASM)

set(ENGINE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/alloc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/easing.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/render.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/image.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/animation.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/font.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/sound.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/map.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/trace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/entity.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/camera.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/scene.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/dither.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/json.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/noise.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/input.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/polygon.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/svg.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/render/render_playdate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/platform/platform_playdate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/sound/sound_playdate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/platform/syscalls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/libs/mod4play/ft_tables.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/libs/mod4play/m4p.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/libs/mod4play/pmp_mix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/libs/mod4play/pmp_main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/libs/mod4play/pmplay.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/libs/mod4play/snd_masm.c
    src/micropolis/g_ani.c
    src/micropolis/rand.c
    src/micropolis/random.c
    src/micropolis/s_alloc.c
    src/micropolis/s_disast.c
    src/micropolis/s_eval.c
    src/micropolis/s_fileio.c
    src/micropolis/s_gen.c
    src/micropolis/s_init.c
    src/micropolis/s_msg.c
    src/micropolis/s_power.c
    src/micropolis/s_scan.c
    src/micropolis/s_sim.c
    src/micropolis/s_traf.c
    src/micropolis/s_zone.c
    src/micropolis/sim_stubs.c
    src/micropolis/w_budget.c
    src/micropolis/w_con.c
    src/micropolis/w_eval.c
    src/micropolis/w_keys.c
    src/micropolis/w_resrc.c
    src/micropolis/w_sound.c
    src/micropolis/w_sprite.c
    src/micropolis/w_tool.c
    src/micropolis/w_update.c
    src/micropolis/w_util.c
    src/sim_render.c
    src/sim_input.c
    src/scene_title.c
    src/game.c
    src/main_playdate.c
)


set(INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/render
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/sound
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/libs
    src
    src/micropolis/headers
)

if (EXISTS ${SDK})
    list(APPEND INCLUDE_DIRS ${SDK}/C_API)
endif()

if (TOOLCHAIN STREQUAL "armgcc")
    add_executable(${PLAYDATE_GAME_DEVICE} ${ENGINE_SOURCES})
    target_compile_definitions(${PLAYDATE_GAME_DEVICE} PUBLIC PLAYDATE=1)
    target_include_directories(${PLAYDATE_GAME_DEVICE} PRIVATE ${INCLUDE_DIRS})
else()
    add_library(${PLAYDATE_GAME_NAME} SHARED ${ENGINE_SOURCES})
    target_compile_definitions(${PLAYDATE_GAME_NAME} PUBLIC PLAYDATE=1)
    target_include_directories(${PLAYDATE_GAME_NAME} PRIVATE ${INCLUDE_DIRS})
endif()

if (EXISTS ${SDK})
    include(${SDK}/C_API/buildsupport/playdate_game.cmake)
endif()
