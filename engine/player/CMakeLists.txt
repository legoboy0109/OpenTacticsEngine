# ote-player: the graphical game runner. The platform backend is chosen at
# configure time (the player sources are identical for both):
#   cmake -S . -B build                      # SDL3 (default)
#   cmake -S . -B build -DOTE_PLATFORM=sdl2  # SDL2 (system package "sdl2")
# Desktop only for now; the web build config (Emscripten ports SDL itself)
# is added by a later task, hence the EMSCRIPTEN guards below.
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

file(GLOB PLAYER_SRC CONFIGURE_DEPENDS *.cpp battle/*.cpp options/*.cpp ui/*.cpp)
# Shared by every backend: text engine, bitmap font, font registry (no SDL inside).
file(GLOB COMMON_SRC CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/engine/platform/common/*.cpp
     ${CMAKE_SOURCE_DIR}/engine/platform/common/*.c)
file(GLOB BACKEND_SRC CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/engine/platform/${OTE_PLATFORM}/*.cpp)

add_executable(ote-player ${PLAYER_SRC} ${COMMON_SRC} ${BACKEND_SRC})
# engine/ for "core/..." and "platform/..."; engine/player for the player's own "battle/..." "ui/...".
target_include_directories(ote-player PRIVATE ${CMAKE_SOURCE_DIR}/engine ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ote-player PRIVATE ote_core)
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
