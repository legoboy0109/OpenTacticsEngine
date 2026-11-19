// SDL2's entry-point shim; see platform/platform_main.h. Where main is renamed (Windows, WinRT, Android) the executable
// must also link SDL2::SDL2main, which engine/player/CMakeLists.txt does.
#pragma once
#include <SDL.h>
#include <SDL_main.h>
