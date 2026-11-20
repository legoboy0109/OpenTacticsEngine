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
# MSVC only: link the C/C++ runtime statically (/MT: nothing to install next to the program, what the Windows desktop
# zip uses) or as the DLL (/MD: needed for UWP and wherever the app brings the Visual C++ runtime itself). docs/WINDOWS.md.
option(OTE_MSVC_STATIC_RUNTIME "MSVC: static runtime (/MT) instead of the DLL runtime (/MD)" ON)
if(MSVC)
  if(OTE_MSVC_STATIC_RUNTIME)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  else()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  endif()
endif()

# UWP (Windows 10/11 and Xbox Dev Mode apps): configure with -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0
# (tools/uwp/build.ps1 does; docs/XBOX.md). Only the player, SDL2 backend, is built for it.
if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
  set(OTE_UWP ON)
  # What Visual Studio's "Windows Store" project type adds (CMake's Ninja generator does not): only the UWP part of the
  # Windows API is declared, so a call outside it is a compile error here and not a failed certification later.
  add_compile_definitions(OTE_UWP=1 WINAPI_FAMILY=WINAPI_FAMILY_APP __WRL_NO_DEFAULT_LIB__)
endif()

# --- Lua 5.4 (vendored, plain C) ---
file(GLOB LUA_SRC third_party/lua/src/*.c)
list(FILTER LUA_SRC EXCLUDE REGEX "/(lua|luac)\\.c$")
add_library(lua STATIC ${LUA_SRC})
target_include_directories(lua PUBLIC third_party/lua/src)
if(UNIX AND NOT EMSCRIPTEN)
  target_compile_definitions(lua PRIVATE LUA_USE_POSIX)
endif()
if(OTE_UWP)
  # Plain C89 Lua: no popen / LoadLibrary (not in the UWP API set). The game scripts only get base, table, string and
  # math anyway (core/script/formulas.cpp).
  target_compile_definitions(lua PRIVATE LUA_USE_C89)
endif()

if(MSVC)
  # Visual Studio / UWP: strict conformance, UTF-8 sources (the engine's strings are UTF-8), and the C library's
  # "unsafe" warnings off. Exceptions stay at /EHsc (the STL needs them declared); the engine itself throws none and
  # uses no RTTI (/GR-). NOMINMAX: windows.h must not define min/max macros. Not built in CI here (no MSVC on Linux).
  set(OTE_WARN /W4 /permissive- /utf-8 /EHsc)
  set(OTE_CXX_FLAGS /GR-)
  set(OTE_NOWARN /w)  # third-party sources compiled into our targets: warnings off for those files only
  add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
else()
  set(OTE_WARN -Wall -Wextra -Wno-unused-parameter)
  set(OTE_CXX_FLAGS -fno-exceptions -fno-rtti)
  set(OTE_NOWARN -w)
endif()

# Sanitizers (docs/TESTING.md "Sanitizer build"): -DOTE_SANITIZE=address, undefined, address,undefined, or thread (not
# together with address). Applied to every target of this project, the vendored Lua included, for compile and link; the
# flags only add run-time checks, no compiler warnings, so the warning sets above are unaffected. Findings are printed and
# the program carries on (no -fno-sanitize-recover), so one run lists them all. GCC and Clang only: on MSVC, Emscripten,
# Android and UWP the option is ignored with a message.
set(OTE_SANITIZE "" CACHE STRING "Sanitizers to build with: address, undefined, address,undefined, thread (empty = off)")
if(OTE_SANITIZE)
  if(MSVC OR EMSCRIPTEN OR ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "WindowsStore"
     OR NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "OTE_SANITIZE='${OTE_SANITIZE}' ignored: sanitizers are only wired for GCC and Clang on desktop systems")
  else()
    message(STATUS "Building with -fsanitize=${OTE_SANITIZE}")
    add_compile_options(-fsanitize=${OTE_SANITIZE} -fno-omit-frame-pointer)
    add_link_options(-fsanitize=${OTE_SANITIZE})
  endif()
endif()

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
# Windows (desktop and UWP): the Direct3D 11 backend too. Its shaders are committed as bytecode headers
# (tools/windows/make_d3d11_shaders.ps1), so no shader compiler is needed to build.
if(WIN32)
  file(GLOB GFX3D_D3D11_SRC CONFIGURE_DEPENDS engine/gfx3d/d3d11/*.cpp)
  list(APPEND GFX3D_SRC ${GFX3D_D3D11_SRC})
endif()
add_library(ote_gfx3d STATIC ${GFX3D_SRC})
target_link_libraries(ote_gfx3d PUBLIC ote_core)
if(WIN32)
  target_link_libraries(ote_gfx3d PUBLIC d3d11 dxgi)
endif()
target_compile_options(ote_gfx3d PRIVATE ${OTE_WARN})

# --- Audio: decoders (WAV, Ogg Vorbis through stb_vorbis), the software mixer, built-in synthesized
# sounds. Portable: memory buffers in, samples out; no device, no file or backend headers (docs/SOUND.md).
file(GLOB AUDIO_SRC CONFIGURE_DEPENDS engine/audio/*.cpp)
add_library(ote_audio STATIC ${AUDIO_SRC} engine/audio/stb_vorbis_impl.c)
target_link_libraries(ote_audio PUBLIC ote_core)
target_compile_options(ote_audio PRIVATE ${OTE_WARN})
# Third-party C: warnings off for this file only.
set_source_files_properties(engine/audio/stb_vorbis_impl.c PROPERTIES COMPILE_OPTIONS "${OTE_NOWARN}")

# --- The player's SDL-free part, compiled once and linked by the player, the tests and the editor helper ---
# Everything the player, ote_tests and ote-editor used to each compile on their own: the player's saves, options,
# UI text and layout, the battle screens' text and views, the sound service and director, and the shared platform code
# that needs no SDL. Each file is compiled once, with the same options every user gave it before (no definition of the
# per-program kind reaches these files: OTE_PLATFORM_*, OTE_SDL2_UPLOAD_ARGB8888 and the test paths are used elsewhere only).
# A STATIC library, not OBJECT: nothing here registers itself from a static initializer (verified: the linked programs
# define the same symbols as before), so the linker pulling only what is referenced is correct, and it lets each program
# keep supplying its own platform functions (ote_tests: tests/platform_stub.cpp; the player: its backend), which the
# library's files call but do not define.
if(OTE_BUILD_TESTS OR OTE_BUILD_PLAYER OR OTE_BUILD_EDITOR)
  set(OTE_PLAYER_DIR ${CMAKE_SOURCE_DIR}/engine/player)
  set(OTE_COMMON_DIR ${CMAKE_SOURCE_DIR}/engine/platform/common)
  set(OTE_PLAYER_LIB_SRC
      ${OTE_COMMON_DIR}/image.cpp ${OTE_COMMON_DIR}/stb_image_impl.cpp ${OTE_COMMON_DIR}/font_registry.cpp ${OTE_COMMON_DIR}/null_audio.cpp ${OTE_COMMON_DIR}/window_fit.cpp
      ${OTE_PLAYER_DIR}/saves.cpp ${OTE_PLAYER_DIR}/progress.cpp ${OTE_PLAYER_DIR}/campaign/campaign_logic.cpp ${OTE_PLAYER_DIR}/options/effective.cpp
      ${OTE_PLAYER_DIR}/ui/text.cpp ${OTE_PLAYER_DIR}/ui/dialogue_text.cpp ${OTE_PLAYER_DIR}/ui/dialogue_box.cpp ${OTE_PLAYER_DIR}/ui/scene_stage.cpp
      ${OTE_PLAYER_DIR}/ui/scene_stage_layout.cpp ${OTE_PLAYER_DIR}/ui/scene_stage_draw.cpp ${OTE_PLAYER_DIR}/ui/dialogue_portrait.cpp
      ${OTE_PLAYER_DIR}/show3d.cpp ${OTE_PLAYER_DIR}/args.cpp ${OTE_PLAYER_DIR}/options/option_defs.cpp ${OTE_PLAYER_DIR}/battle/map_proj.cpp
      # The sound loader, cue lookup, music director and service (docs/SOUND.md).
      ${OTE_PLAYER_DIR}/sound/audio_service.cpp ${OTE_PLAYER_DIR}/sound/audio_log.cpp ${OTE_PLAYER_DIR}/sound/audio_files.cpp ${OTE_PLAYER_DIR}/sound/audio_selftest.cpp
      ${OTE_PLAYER_DIR}/sound/audio_data.cpp ${OTE_PLAYER_DIR}/sound/cues.cpp ${OTE_PLAYER_DIR}/sound/sound_bank.cpp ${OTE_PLAYER_DIR}/sound/music_director.cpp
      ${OTE_PLAYER_DIR}/sound/game_audio.cpp
      # The strategy-RPG shared places and the texts of the battle screens, free of SDL (docs/SRPG.md, docs/BATTLE_SCENE.md).
      ${OTE_PLAYER_DIR}/battle/battle_music.cpp ${OTE_PLAYER_DIR}/battle/promotion_text.cpp ${OTE_PLAYER_DIR}/battle/rank_text.cpp ${OTE_PLAYER_DIR}/battle/staff_text.cpp
      ${OTE_PLAYER_DIR}/battle/condition_text.cpp ${OTE_PLAYER_DIR}/battle/place_text.cpp ${OTE_PLAYER_DIR}/battle/menu_hide.cpp ${OTE_PLAYER_DIR}/battle/weapon_choice.cpp
      ${OTE_PLAYER_DIR}/battle/mapweapon_text.cpp ${OTE_PLAYER_DIR}/battle/fight_script.cpp ${OTE_PLAYER_DIR}/battle/fight_timeline.cpp)
  file(GLOB PLAYER_SRPG_LIB_SRC CONFIGURE_DEPENDS ${OTE_PLAYER_DIR}/battle/srpg_pick.cpp ${OTE_PLAYER_DIR}/battle/unit_info*.cpp
       ${OTE_PLAYER_DIR}/battle/battle_view*.cpp ${OTE_PLAYER_DIR}/battle/economy_text.cpp ${OTE_PLAYER_DIR}/battle/item_text.cpp
       ${OTE_PLAYER_DIR}/battle/rescue_menu.cpp ${OTE_PLAYER_DIR}/battle/support_text.cpp)
  list(APPEND OTE_PLAYER_LIB_SRC ${PLAYER_SRPG_LIB_SRC})
  add_library(ote_player_lib STATIC ${OTE_PLAYER_LIB_SRC})
  target_include_directories(ote_player_lib PUBLIC ${OTE_PLAYER_DIR})  # options/, show3d.h, "battle/..." "ui/..."
  target_link_libraries(ote_player_lib PUBLIC ote_core ote_gfx3d ote_audio)
  target_compile_options(ote_player_lib PRIVATE ${OTE_WARN})
  if(ANDROID)
    # Ends up inside the shared libmain.so (engine/player/CMakeLists.txt turns the global setting on only after this point).
    set_target_properties(ote_player_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)
  endif()
endif()

if(OTE_BUILD_TESTS AND NOT EMSCRIPTEN)
  enable_testing()
  file(GLOB TEST_SRC CONFIGURE_DEPENDS tests/*.cpp)
  # The player's SDL-free sources (saves, options, UI text, sound, battle texts, the PNG decoder ...) come from
  # ote_player_lib; tests/platform_stub.cpp provides the platform functions they call (makeDir, removeFile, commitUserData).
  add_executable(ote_tests ${TEST_SRC})
  target_link_libraries(ote_tests PRIVATE ote_player_lib ote_core ote_gfx3d ote_audio)
  if(WIN32)
    # The Direct3D 11 backend against the software one, its screen quad, state handling and device loss (Windows only;
    # tests/render3d_d3d11_test.py covers it inside the player).
    file(GLOB TEST_WIN_SRC CONFIGURE_DEPENDS tests/windows/*.cpp)
    target_sources(ote_tests PRIVATE ${TEST_WIN_SRC})
    target_include_directories(ote_tests PRIVATE tests)
  endif()
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
    # The content/user roots: the player run from another working directory (docs/PORTING.md).
    add_test(NAME roots_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/roots_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Suspend / resume / low memory / terminate injected with --input tokens (docs/PORTING.md "Life cycle").
    add_test(NAME lifecycle_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/lifecycle_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Cancel on the launcher's first screen: nothing / leave / ask first (platform.h launcherBack()).
    add_test(NAME launcher_back_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/launcher_back_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    add_test(NAME freebattles_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/freebattles_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    add_test(NAME parties_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/parties_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    add_test(NAME testplay_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/testplay_player_test.py
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
    add_test(NAME ai_scripted_sim
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ai_scripted_sim_test.py ${CMAKE_SOURCE_DIR}
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
    # A staff item's own heal sound ("sounds": { "heal": ... }), on the map and in the battle scene (docs/SOUND.md "Overrides").
    add_test(NAME staff_heal_sound_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/staff_heal_sound_player_test.py
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
    # AI behaviour (player, windowless).
    add_test(NAME ai_behaviour_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ai_behaviour_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Scripted AI (player, windowless).
    add_test(NAME ai_scripted_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ai_scripted_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Battle effects (player, windowless).
    add_test(NAME battle_effects_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/battle_effects_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Battle scene (player, windowless).
    add_test(NAME battle_scene_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/battle_scene_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Battle scene, 3D (player, windowless).
    add_test(NAME battle_scene3_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/battle_scene3_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Battle stage (player, windowless).
    add_test(NAME battle_stage_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/battle_stage_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Battle scene, fourth batch of opt-in keys (player, windowless).
    add_test(NAME battle_scene4_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/battle_scene4_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Battle scene, fifth batch of opt-in keys (player, windowless).
    add_test(NAME battle_scene5_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/battle_scene5_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Choices in scenes (player, windowless).
    add_test(NAME choice_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/choice_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Difficulty (player, windowless).
    add_test(NAME difficulty_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/difficulty_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Map changes (player, windowless).
    add_test(NAME mapchange_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/mapchange_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Options menu (player, windowless).
    add_test(NAME menu_options_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/menu_options_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Music (player, windowless).
    add_test(NAME music_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/music_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # 3D backends (player, windowless).
    add_test(NAME render3d_backends
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/render3d_backends_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # The Direct3D 11 backend in the player: feature level 10_1, device loss, multisampling, SDL's back buffer
    # (Windows only: elsewhere it exits 77 and ctest lists it as skipped).
    add_test(NAME render3d_d3d11
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/render3d_d3d11_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    set_tests_properties(render3d_d3d11 PROPERTIES SKIP_RETURN_CODE 77)
    # SRPG extras (player, windowless).
    add_test(NAME srpg_player_extras
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/srpg_player_extras_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # SRPG fidelity (player, windowless).
    add_test(NAME srpg_player_fidelity
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/srpg_player_fidelity_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # SRPG fidelity 2 (player, windowless).
    add_test(NAME srpg_player_fidelity2
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/srpg_player_fidelity2_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # SRPG fidelity 3 (player, windowless).
    add_test(NAME srpg_player_fidelity3
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/srpg_player_fidelity3_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Map weapons (player, windowless).
    add_test(NAME srpg_player_mapweapon
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/srpg_player_mapweapon_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Skill blocks (player, windowless).
    add_test(NAME srpg_player_skillblocks
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/srpg_player_skillblocks_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Skill blocks 2 (player, windowless).
    add_test(NAME srpg_player_skillblocks2
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/srpg_player_skillblocks2_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Stage (player, windowless).
    add_test(NAME stage_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/stage_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # 3D terrain (player, windowless).
    add_test(NAME terrain3d_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/terrain3d_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
    # Walking (player, windowless).
    add_test(NAME walk_player
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/walk_player_test.py
                     $<TARGET_FILE:ote-player> ${CMAKE_SOURCE_DIR})
  endif()
endif()

# --- Editor helper (desktop tool: HTTP server for the browser editor) ---
# Unlike the engine it may use exceptions/threads/<filesystem>; handlers catch
# everything so no error can take the process down.
if(OTE_BUILD_EDITOR AND NOT EMSCRIPTEN)
  file(GLOB EDITOR_SRC CONFIGURE_DEPENDS editor/server/*.cpp)
  add_executable(ote-editor ${EDITOR_SRC})
  # The player's sound loader and service (decoders, silent mode, no device) come from ote_player_lib: /problems runs the
  # same checks as the player so an unknown sound id or an over-long effect is reported before Play (docs/SOUND.md).
  target_link_libraries(ote-editor PRIVATE ote_player_lib ote_core ote_audio)
  target_include_directories(ote-editor PRIVATE engine/player)
  # httplib as SYSTEM include: its warnings are not ours to fix.
  target_include_directories(ote-editor SYSTEM PRIVATE third_party/httplib)
  target_compile_options(ote-editor PRIVATE ${OTE_WARN})
  # ote_core exports -fno-exceptions -fno-rtti as PUBLIC options and those come
  # after our own target options, so -fexceptions is set as a per-source flag,
  # which is placed last on the command line and therefore wins.
  # (MSVC: /EHsc is already on for every target, nothing to add.)
  if(NOT MSVC)
    set_source_files_properties(${EDITOR_SRC} PROPERTIES COMPILE_FLAGS "-fexceptions")
  endif()
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
