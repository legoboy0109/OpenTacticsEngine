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

if(OTE_BUILD_TESTS AND NOT EMSCRIPTEN)
  enable_testing()
  file(GLOB TEST_SRC CONFIGURE_DEPENDS tests/*.cpp)
  # The PNG decoder (platform layer, no SDL) is tested here too.
  add_executable(ote_tests ${TEST_SRC} engine/platform/common/image.cpp engine/platform/common/stb_image_impl.cpp)
  target_link_libraries(ote_tests PRIVATE ote_core)
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

# --- Editor helper (desktop tool: HTTP server for the browser editor) ---
# Unlike the engine it may use exceptions/threads/<filesystem>; handlers catch
# everything so no error can take the process down.
if(OTE_BUILD_EDITOR AND NOT EMSCRIPTEN)
  file(GLOB EDITOR_SRC CONFIGURE_DEPENDS editor/server/*.cpp)
  add_executable(ote-editor ${EDITOR_SRC})
  target_link_libraries(ote-editor PRIVATE ote_core)
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
