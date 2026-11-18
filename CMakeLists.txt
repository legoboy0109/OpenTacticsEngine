cmake_minimum_required(VERSION 3.20)
project(OpenTacticsEngine LANGUAGES C CXX)

# Conservative C++17, no exceptions/RTTI: the engine must build with old
# homebrew console toolchains (see docs/ARCHITECTURE.md "Portability rules").
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
if(NOT CMAKE_BUILD_TYPE AND NOT EMSCRIPTEN)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(OTE_BUILD_PLAYER "Build the SDL3 player" ON)
option(OTE_BUILD_TESTS "Build tests and the headless sim tool" ON)
option(OTE_BUILD_EDITOR "Build the editor helper" ON)

# --- Lua 5.4 (vendored, plain C) ---
file(GLOB LUA_SRC third_party/lua/src/*.c)
list(FILTER LUA_SRC EXCLUDE REGEX "/(lua|luac)\\.c$")
add_library(lua STATIC ${LUA_SRC})
target_include_directories(lua PUBLIC third_party/lua/src)
if(UNIX AND NOT EMSCRIPTEN)
  target_compile_definitions(lua PRIVATE LUA_USE_POSIX)
endif()

set(OTE_WARN -Wall -Wextra -Wno-unused-parameter)
set(OTE_CXX_FLAGS -fno-exceptions -fno-rtti)

# --- Rules core: pure logic, no platform/graphics dependencies ---
file(GLOB_RECURSE CORE_SRC CONFIGURE_DEPENDS engine/core/*.cpp)
add_library(ote_core STATIC ${CORE_SRC})
target_include_directories(ote_core PUBLIC engine)
target_link_libraries(ote_core PUBLIC lua)
target_compile_options(ote_core PRIVATE ${OTE_WARN} PUBLIC ${OTE_CXX_FLAGS})

# --- 3D drawing layer: interface, maths, model loading, the software backend and the
# OpenGL backend. No platform dependencies and no GL headers or libraries: the OpenGL
# backend calls through pointers the platform hands it (docs/RENDER3D.md).
file(GLOB GFX3D_SRC CONFIGURE_DEPENDS engine/gfx3d/*.cpp engine/gfx3d/gl/*.cpp)
add_library(ote_gfx3d STATIC ${GFX3D_SRC})
target_link_libraries(ote_gfx3d PUBLIC ote_core)
target_compile_options(ote_gfx3d PRIVATE ${OTE_WARN})

# --- Audio: decoders (WAV, Ogg Vorbis through stb_vorbis), the software mixer, built-in synthesized
# sounds. Portable: memory buffers in, samples out; no device, no file or backend headers (docs/SOUND.md).
file(GLOB AUDIO_SRC CONFIGURE_DEPENDS engine/audio/*.cpp)
add_library(ote_audio STATIC ${AUDIO_SRC} engine/audio/stb_vorbis_impl.c)
target_link_libraries(ote_audio PUBLIC ote_core)
target_compile_options(ote_audio PRIVATE ${OTE_WARN})
# Third-party C: warnings off for this file only.
set_source_files_properties(engine/audio/stb_vorbis_impl.c PROPERTIES COMPILE_OPTIONS "-w")

if(OTE_BUILD_TESTS AND NOT EMSCRIPTEN)
  enable_testing()
  file(GLOB TEST_SRC CONFIGURE_DEPENDS tests/*.cpp)
  # The PNG decoder (platform layer, no SDL), the player's save-file module
  # (it needs makeDir/removeFile/commitUserData: tests/platform_stub.cpp provides them), the option layering
  # and the 3D picture sizing are tested here too.
  add_executable(ote_tests ${TEST_SRC} engine/platform/common/image.cpp engine/platform/common/stb_image_impl.cpp
                 engine/player/saves.cpp engine/player/campaign/campaign_logic.cpp engine/player/options/effective.cpp engine/platform/common/font_registry.cpp
                 engine/player/ui/text.cpp engine/player/ui/dialogue_text.cpp engine/player/ui/dialogue_box.cpp engine/player/ui/scene_stage.cpp engine/player/ui/scene_stage_layout.cpp engine/player/ui/scene_stage_draw.cpp engine/player/ui/dialogue_portrait.cpp
                 engine/player/show3d.cpp engine/player/sound/audio_service.cpp engine/player/sound/audio_log.cpp
                 engine/player/sound/audio_files.cpp engine/player/sound/audio_selftest.cpp engine/platform/common/null_audio.cpp
                 engine/player/args.cpp engine/player/options/option_defs.cpp engine/player/battle/map_proj.cpp)
  # The player's sound data, cue lookup and music director (docs/SOUND.md step 2).
  target_sources(ote_tests PRIVATE engine/player/sound/audio_data.cpp engine/player/sound/cues.cpp engine/player/sound/sound_bank.cpp
                 engine/player/sound/music_director.cpp engine/player/sound/game_audio.cpp)
  # The player's strategy-RPG shared places (docs/SRPG.md step U0): the pick list, the unit details frame and the
  # battle view with its event hooks, all of it free of SDL.
  file(GLOB PLAYER_SRPG_TEST_SRC CONFIGURE_DEPENDS engine/player/battle/srpg_pick.cpp engine/player/battle/unit_info*.cpp
       engine/player/battle/battle_view*.cpp engine/player/battle/economy_text.cpp engine/player/battle/item_text.cpp
       engine/player/battle/rescue_menu.cpp engine/player/battle/support_text.cpp)
  target_sources(ote_tests PRIVATE ${PLAYER_SRPG_TEST_SRC})
  target_sources(ote_tests PRIVATE engine/player/battle/battle_music.cpp)  # what the music director is told at a turn start (docs/SOUND.md)
  target_sources(ote_tests PRIVATE engine/player/battle/promotion_text.cpp)  # step U2: what a promotion gives, as text
  target_sources(ote_tests PRIVATE engine/player/battle/rank_text.cpp)  # weapon ranks and stat caps in words (docs/SRPG.md section 7)
  target_sources(ote_tests PRIVATE engine/player/battle/staff_text.cpp)  # staves in words (docs/SRPG.md section 8)
  target_sources(ote_tests PRIVATE engine/player/battle/condition_text.cpp)  # status conditions in words (docs/SRPG.md section 9)
  target_sources(ote_tests PRIVATE engine/player/battle/place_text.cpp)  # doors, chests and visiting in words (docs/SRPG.md section 10)
  target_sources(ote_tests PRIVATE engine/player/battle/mapweapon_text.cpp)  # map weapons in words (docs/SRPG.md "Map weapons")
  target_sources(ote_tests PRIVATE engine/player/battle/fight_script.cpp engine/player/battle/fight_timeline.cpp)  # the battle scene's script of a fight (docs/BATTLE_SCENE.md)
  target_link_libraries(ote_tests PRIVATE ote_core ote_gfx3d ote_audio)
  target_include_directories(ote_tests PRIVATE engine/player)  # options/, show3d.h
  target_compile_options(ote_tests PRIVATE ${OTE_WARN})
  target_compile_definitions(ote_tests PRIVATE OTE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
                                             OTE_TEMP_DIR="${CMAKE_BINARY_DIR}")
  add_test(NAME ote_tests COMMAND ote_tests)

  file(GLOB SIM_SRC CONFIGURE_DEPENDS tools/sim/*.cpp)
  if(SIM_SRC)
    add_executable(ote-sim ${SIM_SRC})
    target_link_libraries(ote-sim PRIVATE ote_core)
    target_compile_options(ote-sim PRIVATE ${OTE_WARN})
  endif()
endif()

if(OTE_BUILD_PLAYER AND EXISTS ${CMAKE_SOURCE_DIR}/engine/player/CMakeLists.txt)
  add_subdirectory(engine/player)
endif()

# The campaign played through the real player, windowless (tests/campaign_player_test.py).
if(OTE_BUILD_PLAYER AND OTE_BUILD_TESTS AND NOT EMSCRIPTEN AND TARGET ote-player)
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_FOUND)
    enable_testing()
    add_test(NAME campaign_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/campaign_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    add_test(NAME parties_player
    add_test(NAME testplay_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/testplay_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/parties_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    add_test(NAME economy_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/economy_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    add_test(NAME demo_economy_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/demo_economy_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    add_test(NAME starter_audio
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/starter_audio_test.py ${CMAKE_SOURCE_DIR})
    add_test(NAME hex_content
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/hex_content_test.py ${CMAKE_SOURCE_DIR}
                     $<TARGET_FILE:ote-sim>)
    # The strategy-RPG content (demo, template, fixtures): sims without stalls, determinism, event printer (docs/SRPG.md D1).
    add_test(NAME srpg_content
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/srpg_content_test.py ${CMAKE_SOURCE_DIR}
                     $<TARGET_FILE:ote-sim>)
    # The unit art of the story demo and the template: every unit type has its own picture / model in every style.
    add_test(NAME srpg_art
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/srpg_art_test.py ${CMAKE_SOURCE_DIR})
    add_test(NAME audio_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/audio_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    add_test(NAME sound_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/sound_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    add_test(NAME fog_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/fog_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Story scene presentation (docs/EVENTS.md "Scene presentation"), windowless.
    add_test(NAME scene_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/scene_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Hexagon maps in the top-down view (docs/HEX.md), windowless.
    add_test(NAME hex_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/hex_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # The same maps in the 2D isometric view (docs/HEX.md "2D isometric").
    add_test(NAME hex_iso_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/hex_iso_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Strategy-RPG skills and unit details in the player (docs/SRPG.md step U1).
    add_test(NAME srpg_player_skills
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/srpg_player_skills_test.py
                      $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Carrying and pair up through the real player (docs/SRPG.md step U4).
    add_test(NAME srpg_player_rescue
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/srpg_player_rescue_test.py
                      $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Supports and the camp between chapters (docs/SRPG.md step U5).
    add_test(NAME srpg_supports_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/srpg_player_supports_test.py
                      $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Strategy-RPG step U3: abilities and summons in the battle (docs/SRPG.md), windowless.
    add_test(NAME srpg_player_abilities
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/srpg_player_abilities_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # The strategy-RPG player: promotion (docs/SRPG.md step U2).
    add_test(NAME srpg_promotion_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/srpg_player_promotion_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Weapon ranks and stat caps in the player (docs/SRPG.md section 7).
    add_test(NAME srpg_player_ranks
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/srpg_player_ranks_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Character-locked weapons in the player (docs/SRPG.md "Character-locked weapons").
    add_test(NAME srpg_player_locks
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/srpg_player_locks_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # The preparation screen in the player (docs/EVENTS.md "Preparation").
    add_test(NAME srpg_player_prep
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/srpg_player_prep_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Staves in the player (docs/SRPG.md section 8): menu, list, target pick, forecast, heal.
    add_test(NAME srpg_player_staves
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/srpg_player_staves_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Staves, stage two (docs/SRPG.md section 8): warp, pull, swap, repair, light; hexagons and every kind of view.
    add_test(NAME srpg_player_staves2
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/srpg_player_staves2_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Status conditions (docs/SRPG.md section 9): staves, items, the turn flow, berserk, the marker in every kind of view.
    add_test(NAME srpg_player_conditions
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/srpg_player_conditions_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Doors, keys, chests and visiting (docs/SRPG.md section 10): menu, key choice, break door, unlock ability and staff, auto setting, fog.
    add_test(NAME srpg_player_doors
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/srpg_player_doors_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Battle and defeat quotes (docs/EVENTS.md): the lines before the strikes and after a fall, the enemy turn, skip, autoplay.
    add_test(NAME srpg_player_quotes
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/srpg_player_quotes_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # And in the two 3D kinds (docs/HEX.md "3D isometric and full 3D"), both 3D backends where there is a driver.
    add_test(NAME hex3d_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/hex3d_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # The tile layer of a map in the four kinds of view (docs/GRAPHICS.md "Tile layer").
    add_test(NAME tiles_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/tiles_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
  endif()
endif()

# --- Editor helper (desktop tool: HTTP server for the browser editor) ---
# Unlike the engine it may use exceptions/threads/<filesystem>; handlers catch
# everything so no error can take the process down.
if(OTE_BUILD_EDITOR AND NOT EMSCRIPTEN)
  file(GLOB EDITOR_SRC CONFIGURE_DEPENDS editor/server/*.cpp)
  add_executable(ote-editor ${EDITOR_SRC})
  target_link_libraries(ote-editor PRIVATE ote_core ote_audio)
  # The player's sound loader and service (decoders, silent mode, no device): /problems runs the same checks as the
  # player so an unknown sound id or an over-long effect is reported before Play (docs/SOUND.md).
  target_sources(ote-editor PRIVATE engine/player/sound/audio_data.cpp engine/player/sound/cues.cpp
                 engine/player/sound/sound_bank.cpp engine/player/sound/music_director.cpp engine/player/sound/game_audio.cpp
                 engine/player/sound/audio_service.cpp engine/player/sound/audio_log.cpp engine/player/sound/audio_files.cpp
                 engine/platform/common/null_audio.cpp)
  target_include_directories(ote-editor PRIVATE engine/player)
  # httplib as SYSTEM include: its warnings are not ours to fix.
  target_include_directories(ote-editor SYSTEM PRIVATE third_party/httplib)
  target_compile_options(ote-editor PRIVATE ${OTE_WARN})
  # ote_core exports -fno-exceptions -fno-rtti as PUBLIC options and those come
  # after our own target options, so -fexceptions is set as a per-source flag,
  # which is placed last on the command line and therefore wins.
  set_source_files_properties(${EDITOR_SRC} PROPERTIES COMPILE_FLAGS "-fexceptions")
  find_package(Threads REQUIRED)
  target_link_libraries(ote-editor PRIVATE Threads::Threads)
  if(WIN32)
    target_link_libraries(ote-editor PRIVATE ws2_32)
  endif()

  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_FOUND AND OTE_BUILD_TESTS)
    enable_testing()
    add_test(NAME editor_api
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/editor_api_test.py
                     $<TARGET_FILE:ote-editor> ${CMAKE_SOURCE_DIR})
  endif()
endif()
