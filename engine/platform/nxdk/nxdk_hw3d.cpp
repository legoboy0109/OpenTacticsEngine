// The original Xbox has no hardware 3D backend yet (a native NV2A one would go here, docs/XBOX_OG.md "Step 2"): the 3D views are
// not offered (player/views.h), and the software drawer is the fallback if something asks for 3D anyway. Same three functions as sdl2_gl3d.cpp.
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
