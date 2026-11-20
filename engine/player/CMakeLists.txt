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

# Android (docs/ANDROID.md): SDL3 is built from the release source tree android/ fetched (tools/android/fetch_sdl.sh),
# as a shared library, and the player becomes libmain.so, the library SDLActivity loads and calls SDL_main in.
set(OTE_ANDROID_SDL_SOURCE "" CACHE PATH "Android: SDL3 source tree to build SDL3 from")
if(ANDROID)
  if(NOT OTE_PLATFORM STREQUAL "sdl3")
    message(FATAL_ERROR "The Android build uses the SDL3 backend")
  endif()
  if(NOT OTE_ANDROID_SDL_SOURCE OR NOT EXISTS "${OTE_ANDROID_SDL_SOURCE}/CMakeLists.txt")
    message(FATAL_ERROR "Android: set OTE_ANDROID_SDL_SOURCE to an SDL3 source tree (run tools/android/fetch_sdl.sh)")
  endif()
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)  # the static engine libraries end up inside a shared library
  set(SDL_SHARED ON CACHE BOOL "" FORCE)
  set(SDL_STATIC OFF CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  add_subdirectory("${OTE_ANDROID_SDL_SOURCE}" "${CMAKE_BINARY_DIR}/sdl3")
elseif(OTE_UWP)
  # UWP (docs/XBOX.md): SDL2 built from its own VisualC-WinRT project (SDL2.lib / SDL2.dll, x64 Release, /MD); SDL has no
  # CMake package for it. OTE_UWP_SDL2_SOURCE = the unpacked SDL2 release tree (include/, src/main/winrt/),
  # OTE_UWP_SDL2_LIBDIR = the folder MSBuild wrote SDL2.lib and SDL2.dll to.
  if(NOT OTE_PLATFORM STREQUAL "sdl2")
    message(FATAL_ERROR "The UWP build uses the SDL2 backend: -DOTE_PLATFORM=sdl2")
  endif()
  set(OTE_UWP_SDL2_SOURCE "" CACHE PATH "UWP: unpacked SDL2 release source tree")
  set(OTE_UWP_SDL2_LIBDIR "" CACHE PATH "UWP: folder with SDL2.lib and SDL2.dll built from VisualC-WinRT")
  if(NOT EXISTS "${OTE_UWP_SDL2_SOURCE}/include/SDL.h" OR NOT EXISTS "${OTE_UWP_SDL2_LIBDIR}/SDL2.lib")
    message(FATAL_ERROR "UWP: set OTE_UWP_SDL2_SOURCE and OTE_UWP_SDL2_LIBDIR (tools/uwp/build.ps1 does)")
  endif()
  add_library(SDL2::SDL2 SHARED IMPORTED)
  set_target_properties(SDL2::SDL2 PROPERTIES IMPORTED_IMPLIB "${OTE_UWP_SDL2_LIBDIR}/SDL2.lib"
                        IMPORTED_LOCATION "${OTE_UWP_SDL2_LIBDIR}/SDL2.dll"
                        INTERFACE_INCLUDE_DIRECTORIES "${OTE_UWP_SDL2_SOURCE}/include")
elseif(NOT EMSCRIPTEN)
  if(OTE_PLATFORM STREQUAL "sdl3")
    find_package(SDL3 REQUIRED CONFIG)
  else()
    find_package(SDL2 REQUIRED CONFIG)
  endif()
endif()

file(GLOB PLAYER_SRC CONFIGURE_DEPENDS *.cpp battle/*.cpp campaign/*.cpp jobs/*.cpp options/*.cpp sound/*.cpp ui/*.cpp)
# Shared by every backend: text engine, bitmap font, font registry (no SDL inside).
file(GLOB COMMON_SRC CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/engine/platform/common/*.cpp
     ${CMAKE_SOURCE_DIR}/engine/platform/common/*.c)
# The SDL-free files ote_player_lib (root CMakeLists.txt) compiles once for the player, the tests and the editor helper.
list(REMOVE_ITEM PLAYER_SRC ${OTE_PLAYER_LIB_SRC})
list(REMOVE_ITEM COMMON_SRC ${OTE_PLAYER_LIB_SRC})
file(GLOB BACKEND_SRC CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/engine/platform/${OTE_PLATFORM}/*.cpp)

if(ANDROID)
  add_library(ote-player SHARED ${PLAYER_SRC} ${COMMON_SRC} ${BACKEND_SRC})
  set_target_properties(ote-player PROPERTIES OUTPUT_NAME main)  # libmain.so
  target_link_libraries(ote-player PRIVATE log EGL GLESv3)  # logcat (sdl_android.cpp); the 3D pbuffer context (sdl_gl3d_android.cpp)
elseif(OTE_UWP)
  # SDL's own WinRT entry point (WinMain -> SDL_WinRTRunApp -> main) is C++/CX and goes into the program, with the one
  # other C++/CX file of ours; both are compiled with /ZW, everything else stays plain C++.
  set(UWP_ZW_SRC ${OTE_UWP_SDL2_SOURCE}/src/main/winrt/SDL_winrt_main_NonXAML.cpp
      ${CMAKE_SOURCE_DIR}/engine/platform/sdl2/sdl2_uwp_view.cpp)
  add_executable(ote-player WIN32 ${PLAYER_SRC} ${COMMON_SRC} ${BACKEND_SRC} ${UWP_ZW_SRC})
  # /ZW needs RTTI-enabled C++ (the engine-wide /GR- and /permissive- are overridden for these two files: vccorlib.h does not parse under /permissive-) and the WinRT metadata
  # (platform.winmd, Windows.winmd), which vcvarsall's "uwp" environment puts on LIBPATH.
  set_source_files_properties(${UWP_ZW_SRC} PROPERTIES COMPILE_OPTIONS "/ZW;/GR;/EHsc;/permissive;/wd4447;/wd4996"
                              SKIP_PRECOMPILE_HEADERS ON)
  target_compile_definitions(ote-player PRIVATE OTE_UWP=1)
  target_compile_definitions(ote-player PRIVATE OTE_SDL2_UPLOAD_ARGB8888=1)  # Direct3D 11 has no RGBA32 texture format
  # vccorlib (C++/CX runtime) must come before the C runtime on the link line (LNK2038), so it is named first and
  # explicitly; runtimeobject is what SDL_winrt_main_NonXAML.cpp asks for with a #pragma.
  target_link_libraries(ote-player PRIVATE vccorlib msvcrt runtimeobject)
else()
  add_executable(ote-player ${PLAYER_SRC} ${COMMON_SRC} ${BACKEND_SRC})
  if(WIN32)
    # GUI subsystem: a double-click opens no console window. SDL's main header supplies WinMain; attachParentConsole()
    # (platform/common/console.cpp) gives the program its terminal back when it was started from one (docs/WINDOWS.md).
    set_target_properties(ote-player PROPERTIES WIN32_EXECUTABLE ON)
  endif()
endif()
# engine/ for "core/..." and "platform/..."; engine/player for the player's own "battle/..." "ui/...".
target_include_directories(ote-player PRIVATE ${CMAKE_SOURCE_DIR}/engine ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ote-player PRIVATE ote_player_lib ote_core ote_gfx3d ote_audio)
if(NOT EMSCRIPTEN)  # (on Android SDL3::SDL3 is the shared library built above)
  if(OTE_PLATFORM STREQUAL "sdl3")
    target_link_libraries(ote-player PRIVATE SDL3::SDL3)
  else()
    # SDL2's CMake config exports SDL2::SDL2 (and SDL2::SDL2main); older
    # packages only set SDL2_LIBRARIES / SDL2_INCLUDE_DIRS.
    if(TARGET SDL2::SDL2)
      # SDL2main supplies the real entry point where SDL_main.h renames main (Windows/UWP); not needed on Linux.
      if(TARGET SDL2::SDL2main AND (WIN32 OR ANDROID))
        target_link_libraries(ote-player PRIVATE SDL2::SDL2main)
      endif()
      target_link_libraries(ote-player PRIVATE SDL2::SDL2)
    else()
      target_include_directories(ote-player SYSTEM PRIVATE ${SDL2_INCLUDE_DIRS})
      target_link_libraries(ote-player PRIVATE ${SDL2_LIBRARIES})
    endif()
  endif()
endif()
# Windows: SDL is a DLL (the "-VC" development package); copy it next to the program so it starts from the build folder.
if(WIN32 AND NOT EMSCRIPTEN AND NOT CMAKE_VERSION VERSION_LESS 3.21)
  add_custom_command(TARGET ote-player POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:ote-player> $<TARGET_FILE_DIR:ote-player>
    COMMAND_EXPAND_LISTS)
endif()
# Which backend's entry-point header platform/platform_main.h pulls in.
if(OTE_PLATFORM STREQUAL "sdl3")
  target_compile_definitions(ote-player PRIVATE OTE_PLATFORM_SDL3=1)
else()
  target_compile_definitions(ote-player PRIVATE OTE_PLATFORM_SDL2=1)
endif()
# SDL2 only: upload textures as ARGB8888 (D3D11's native format, the UWP renderer) instead of RGBA32, converting the
# bytes once in the backend (engine/platform/sdl2/sdl2_pixels.h). Default OFF = unchanged.
option(OTE_SDL2_UPLOAD_ARGB8888 "SDL2 backend: ARGB8888 textures (for Direct3D 11, UWP)" OFF)
if(OTE_SDL2_UPLOAD_ARGB8888 AND OTE_PLATFORM STREQUAL "sdl2")
  target_compile_definitions(ote-player PRIVATE OTE_SDL2_UPLOAD_ARGB8888=1)
endif()
target_compile_options(ote-player PRIVATE ${OTE_WARN})
# Third-party sources compiled into the player: warnings off for these files only.
set_source_files_properties(${CMAKE_SOURCE_DIR}/engine/platform/common/bitmap_font_data.c
                            ${CMAKE_SOURCE_DIR}/engine/platform/common/stb_truetype_impl.cpp
                            PROPERTIES COMPILE_OPTIONS "${OTE_NOWARN}")

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
    # SDL's software renderer (a browser without WebGL: acceleration off or blocked) shows its picture through
    # Emscripten's Browser helper, which nothing else pulls in: without this line that browser stops with
    # "Browser is not defined".
    "-sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE=$Browser"
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
