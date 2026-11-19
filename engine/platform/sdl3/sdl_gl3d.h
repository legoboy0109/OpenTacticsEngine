// The GL context of the hardware 3D backend on SDL3 (and on the web).
#pragma once

#include <memory>
#include <string>

#include "gfx3d/render3d.h"

namespace ote {

// Frees the context; called by runApp before SDL goes away. Backends made
// by createHardwareRenderer3D stay valid and draw nothing afterwards.
void shutdownHardware3D();

#ifdef __ANDROID__
// sdl_gl3d_android.cpp: the Android hardware backend (EGL pbuffer context, rebuilt if lost; see that file).
std::unique_ptr<Renderer3D> createAndroidHardwareRenderer3D(std::string& why);
void shutdownAndroidHardware3D();
#endif

}  // namespace ote
