// The original Xbox's hardware 3D backend (docs/RENDER3D.md "Original Xbox backend"): Renderer3D on the NV2A through pbkit,
// hw3d_renderer.*. Same three functions as sdl2_gl3d.cpp.
#include <memory>
#include <string>

#include "gfx3d/render3d.h"
#include "platform/nxdk/hw3d_gpu.h"
#include "platform/nxdk/hw3d_renderer.h"
#include "platform/platform3d.h"
#include "platform/sdl2/sdl2_gl3d.h"

namespace ote {

void attachHardware3D(SDL_Renderer*) {}
void shutdownHardware3D() {}

// The first call starts the chip (pbkit takes the screen over, nxdk_present.h): from then on it shows every frame.
std::unique_ptr<Renderer3D> createHardwareRenderer3D(std::string& why) {
  if (!hw3d::start(why)) return nullptr;
  return std::unique_ptr<Renderer3D>(new NxdkRenderer3D());
}

}  // namespace ote
