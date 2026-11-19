// The hardware 3D backends on SDL3: the GL context (desktop, web, Android) and, on Windows, Direct3D 11.
#pragma once

#include <memory>
#include <string>

#include "gfx3d/render3d.h"

struct SDL_Renderer;

namespace ote {

// The 2D renderer the player draws with (runApp, right after making it). On Windows a Direct3D 11 one lends the 3D
// backend its device; elsewhere this does nothing.
void attachHardware3D(SDL_Renderer* renderer);

// Frees the context; called by runApp before SDL goes away. Backends made
// by createHardwareRenderer3D stay valid and draw nothing afterwards.
void shutdownHardware3D();

#ifdef _WIN32
// sdl_d3d11_3d.cpp: the Direct3D 11 backend on SDL's device or an own one (rebuilt if lost); tried before OpenGL.
void attachD3D11Renderer(SDL_Renderer* renderer);
std::unique_ptr<Renderer3D> createD3D11HardwareRenderer3D(std::string& why);
void shutdownD3D11Hardware3D();
#endif

#ifdef __ANDROID__
// sdl_gl3d_android.cpp: the Android hardware backend (EGL pbuffer context, rebuilt if lost; see that file).
std::unique_ptr<Renderer3D> createAndroidHardwareRenderer3D(std::string& why);
void shutdownAndroidHardware3D();
#endif

}  // namespace ote
