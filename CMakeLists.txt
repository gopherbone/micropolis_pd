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
    engine/core/render.c
    engine/core/image.c
    engine/core/font.c
    engine/core/sound.c
    engine/core/scene.c
    engine/platform/platform_playdate.c
    engine/platform/syscalls.c
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
    src/ui.c
    src/map_view.c
    src/screens.c
    src/sim_input.c
    src/scene_title.c
    src/game.c
    src/main_playdate.c
)


set(INCLUDE_DIRS
    engine/core
    engine/platform
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
