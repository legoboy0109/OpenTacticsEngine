// Include this in the one file that defines main() (engine/player/main.cpp) and nowhere else.
// SDL on Android, iOS, UWP/WinRT and Windows (GUI subsystem) starts the program through its own entry point and calls
// main() from there; its header (SDL_main.h) renames main to SDL_main where that is needed and is a no-op on desktop
// Linux/macOS. Which header depends on the backend, so the choice is made here, inside the platform layer. The web build
// is left exactly as it was (no SDL_main).
#pragma once
#if defined(__EMSCRIPTEN__)
// nothing: Emscripten calls main() itself
#elif defined(OTE_PLATFORM_SDL3)
#include "platform/sdl3/sdl_main.h"
#elif defined(OTE_PLATFORM_SDL2)
#include "platform/sdl2/sdl2_main.h"
#endif
