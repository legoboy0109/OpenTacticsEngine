# ote-player: the graphical game runner (SDL3 backend + scenes).
# Desktop only for now; the web build config (Emscripten ports SDL3 itself)
# is added by a later task, hence the EMSCRIPTEN guard below.
if(NOT EMSCRIPTEN)
  find_package(SDL3 REQUIRED CONFIG)
endif()

file(GLOB PLAYER_SRC CONFIGURE_DEPENDS *.cpp battle/*.cpp ui/*.cpp)
file(GLOB SDL3_BACKEND_SRC CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/engine/platform/sdl3/*.cpp)

add_executable(ote-player ${PLAYER_SRC} ${SDL3_BACKEND_SRC})
# engine/ for "core/..." and "platform/..."; engine/player for the player's own "battle/..." "ui/...".
target_include_directories(ote-player PRIVATE ${CMAKE_SOURCE_DIR}/engine ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ote-player PRIVATE ote_core)
if(NOT EMSCRIPTEN)
  target_link_libraries(ote-player PRIVATE SDL3::SDL3)
endif()
target_compile_options(ote-player PRIVATE ${OTE_WARN})
