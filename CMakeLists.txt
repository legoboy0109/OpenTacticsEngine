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
  add_executable(ote_tests ${TEST_SRC})
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
