// See nxdk_prof.h.
#include "platform/nxdk/nxdk_prof.h"

#include <cstdio>
#include <cstring>

namespace ote {
namespace prof {

double g_ticks[kSlots];
double g_count[kCounters];

void format(char* out, int size, int frames) {
  static const char* const kNames[kSlots] = {"update", "draw",     "overlay", "sdl2d", "r3dBegin", "r3dDraw",
                                             "r3dEnd", "meshFill", "chipWait", "clip", "layer", "present",  "vbl"};
  static const char* const kCountNames[kCounters] = {"layerKB", "sdlKpix", "layers", "draws", "idxWords", "meshUpd", "meshKB", "tris", "cutMeshes", "outside", "cutReused"};
  const double hz = (double)SDL_GetPerformanceFrequency();
  int n = 0;
  for (int i = 0; i < kSlots && n < size - 40; ++i) {
    const int h = (int)(1e5 * g_ticks[i] / hz / frames);  // hundredths of a millisecond
    n += snprintf(out + n, (size_t)(size - n), "%s%s %d.%02d", i ? ", " : "", kNames[i], h / 100, h % 100);
  }
  if (n >= size - 40) n = size - 41;
  n += snprintf(out + n, (size_t)(size - n), " ms | per frame:");
  for (int i = 0; i < kCounters && n < size - 40; ++i) {
    double v = g_count[i];
    if (i == kLayerBytes || i == kMeshBytes || i == kSdlPixels) v /= 1024.0;
    const int h = (int)(100.0 * v / frames);
    n += snprintf(out + n, (size_t)(size - n), " %s %d.%02d", kCountNames[i], h / 100, h % 100);
  }
  (void)n;
  memset(g_ticks, 0, sizeof(g_ticks));
  memset(g_count, 0, sizeof(g_count));
}

}  // namespace prof
}  // namespace ote
