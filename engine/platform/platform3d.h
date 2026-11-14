// The platform's faster 3D backend, if it has one (docs/RENDER3D.md). Kept
// apart from platform.h so the 2D-only parts of the player need not know the
// 3D layer.
#pragma once
#include <memory>
#include <string>

#include "gfx3d/render3d.h"

namespace ote {

// A backend that draws with the graphics chip, or null with a plain reason
// in `why` (the caller then uses the software backend). Call after the
// platform has started (from App::onStart or later). The result may outlive
// the platform: once the platform has shut down it simply draws nothing.
std::unique_ptr<Renderer3D> createHardwareRenderer3D(std::string& why);

}  // namespace ote
