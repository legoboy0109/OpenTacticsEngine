// The GL context of the hardware 3D backend on SDL2.
#pragma once

namespace ote {

// Frees the context; called by runApp before SDL goes away. Backends made
// by createHardwareRenderer3D stay valid and draw nothing afterwards.
void shutdownHardware3D();

}  // namespace ote
