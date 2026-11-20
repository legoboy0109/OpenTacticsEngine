// STUB for the original-Xbox link test: no hardware 3D backend exists (a native NV2A one would go here, see
// FINDINGS.md), so the player falls back to the software rasterizer. Same three functions sdl2_gl3d.cpp defines.
#include <memory>
#include <string>

#include "gfx3d/render3d.h"
#include "platform/platform3d.h"
#include "platform/sdl2/sdl2_gl3d.h"

namespace ote {

void attachHardware3D(SDL_Renderer*) {}
void shutdownHardware3D() {}
std::unique_ptr<Renderer3D> createHardwareRenderer3D(std::string& why) {
  why = "no hardware 3D backend on the original Xbox yet";
  return nullptr;
}

}  // namespace ote
