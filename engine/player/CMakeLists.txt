# ote-player: the graphical game runner. The platform backend is chosen at
# configure time (the player sources are identical for both):
#   cmake -S . -B build                      # SDL3 (default)
#   cmake -S . -B build -DOTE_PLATFORM=sdl2  # SDL2 (system package "sdl2")
# Web: `emcmake cmake -S . -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release`
# builds ote-player.js/.wasm (SDL3 from Emscripten's port) plus the pages from
# engine/platform/web and a copy of games/ + assets/ with a manifest, all in
# build-web/engine/player (see docs/ARCHITECTURE.md "Web").
set(OTE_PLATFORM "sdl3" CACHE STRING "Platform backend: sdl3 | sdl2")
set_property(CACHE OTE_PLATFORM PROPERTY STRINGS sdl3 sdl2)
if(NOT OTE_PLATFORM STREQUAL "sdl3" AND NOT OTE_PLATFORM STREQUAL "sdl2")
  message(FATAL_ERROR "OTE_PLATFORM must be sdl3 or sdl2 (got '${OTE_PLATFORM}')")
endif()

if(NOT EMSCRIPTEN)
  if(OTE_PLATFORM STREQUAL "sdl3")
    find_package(SDL3 REQUIRED CONFIG)
  else()
    find_package(SDL2 REQUIRED CONFIG)
  endif()
endif()

file(GLOB PLAYER_SRC CONFIGURE_DEPENDS *.cpp battle/*.cpp campaign/*.cpp options/*.cpp sound/*.cpp ui/*.cpp)
# Shared by every backend: text engine, bitmap font, font registry (no SDL inside).
file(GLOB COMMON_SRC CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/engine/platform/common/*.cpp
     ${CMAKE_SOURCE_DIR}/engine/platform/common/*.c)
file(GLOB BACKEND_SRC CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/engine/platform/${OTE_PLATFORM}/*.cpp)

add_executable(ote-player ${PLAYER_SRC} ${COMMON_SRC} ${BACKEND_SRC})
# engine/ for "core/..." and "platform/..."; engine/player for the player's own "battle/..." "ui/...".
target_include_directories(ote-player PRIVATE ${CMAKE_SOURCE_DIR}/engine ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ote-player PRIVATE ote_core ote_gfx3d ote_audio)
if(NOT EMSCRIPTEN)
  if(OTE_PLATFORM STREQUAL "sdl3")
    target_link_libraries(ote-player PRIVATE SDL3::SDL3)
  else()
    # SDL2's CMake config exports SDL2::SDL2 (and SDL2::SDL2main); older
    # packages only set SDL2_LIBRARIES / SDL2_INCLUDE_DIRS.
    if(TARGET SDL2::SDL2)
      target_link_libraries(ote-player PRIVATE SDL2::SDL2)
    else()
      target_include_directories(ote-player SYSTEM PRIVATE ${SDL2_INCLUDE_DIRS})
      target_link_libraries(ote-player PRIVATE ${SDL2_LIBRARIES})
    endif()
  endif()
endif()
target_compile_options(ote-player PRIVATE ${OTE_WARN})
# Third-party sources compiled into the player: warnings off for these files only.
set_source_files_properties(${CMAKE_SOURCE_DIR}/engine/platform/common/bitmap_font_data.c
                            ${CMAKE_SOURCE_DIR}/engine/platform/common/stb_truetype_impl.cpp
                            PROPERTIES COMPILE_OPTIONS "-w")

if(EMSCRIPTEN)
  if(NOT OTE_PLATFORM STREQUAL "sdl3")
    message(FATAL_ERROR "The web build uses the SDL3 backend (SDL2's Emscripten branch is untested)")
  endif()
  target_compile_options(ote-player PRIVATE --use-port=sdl3)
  # MODULARIZE: the page gets a factory (createOtePlayer) and decides the
  # arguments (INVOKE_RUN=0: main is started with Module.callMain).
  # IDBFS holds the settings between visits. No exceptions/threads in the engine.
  target_link_options(ote-player PRIVATE
    --use-port=sdl3 -O2
    -sMODULARIZE=1 -sEXPORT_NAME=createOtePlayer -sENVIRONMENT=web
    -sINVOKE_RUN=0 -sEXIT_RUNTIME=0
    -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576
    -sFORCE_FILESYSTEM=1 -lidbfs.js
    -sEXPORTED_RUNTIME_METHODS=FS,callMain,IDBFS
    # The hardware 3D backend: WebGL 2, its functions looked up by name.
    -sMAX_WEBGL_VERSION=2 -sGL_ENABLE_GET_PROC_ADDRESS=1)

  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  # Pages + data next to the module, so the folder can be served as it is.
  set(WEB_SRC ${CMAKE_SOURCE_DIR}/engine/platform/web)
  set(WEB_OUT $<TARGET_FILE_DIR:ote-player>)
  add_custom_target(ote-web-files ALL
    COMMAND ${CMAKE_COMMAND} -E copy ${WEB_SRC}/index.html ${WEB_SRC}/embed.html ${WEB_SRC}/player.js
            ${WEB_SRC}/player.css ${WEB_OUT}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/games ${WEB_OUT}/games
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets ${WEB_OUT}/assets
    COMMAND ${Python3_EXECUTABLE} ${WEB_SRC}/make_manifest.py ${WEB_OUT} ${WEB_OUT}/games.manifest.json
    VERBATIM)
  add_dependencies(ote-web-files ote-player)
endif()
