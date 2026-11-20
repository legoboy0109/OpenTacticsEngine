// Frame-time breakdown for the original Xbox port (docs/XBOX_OG.md "Speed"). Cheap accumulators (one performance-counter read
// at each end of a timed span) that the player's debug line `frame prof:` prints every 120 frames when OTE_LOG_LEVEL=debug.
// Spans may nest (the line says which include which); nothing here changes what is drawn.
#pragma once
#include <SDL.h>

namespace ote {
namespace prof {

enum Slot {
  kUpdate,     // App::update (game logic, AI, animation)
  kDraw,       // App::draw: everything the game draws, 2D and 3D, including the spans below
  kOverlay,    // the debug overlay (only with OTE_XBOX_OVERLAY)
  kSdl2d,      // SDL's CPU renderer: fills, glyph and sprite copies (inside kDraw)
  kR3dBegin,   // Renderer3D::begin: waiting for the chip, putting the 2D layer so far down, clears (inside kDraw)
  kR3dDraw,    // Renderer3D::draw: matrices, visibility test, state and index words (inside kDraw)
  kR3dEnd,     // Renderer3D::end: the see-through meshes (inside kDraw)
  kMeshFill,   // createMesh / updateMesh: copies into chip memory (inside kDraw)
  kChipWait,   // spins until the chip has finished (inside several of the others)
  kClip,       // cutting meshes at the camera on the CPU (inside kR3dDraw)
  kLayer,      // the 2D layer's copy into a texture and its rectangle (inside kR3dBegin or kPresent)
  kPresent,    // nxdkPresent without the wait for the vertical blank
  kVblank,     // waiting for the vertical blank (the pacing)
  kSlots
};
enum Counter {
  kLayerBytes,   // bytes copied into layer textures
  kSdlPixels,    // destination pixels of SDL's fills and copies (the 2D layer's cost in pixels)
  kLayers,       // layer rectangles drawn
  kDrawCalls,    // meshes sent to the chip
  kIndexWords,   // 32-bit words of indices written into the push buffer
  kMeshWrites,   // updateMesh calls
  kMeshBytes,    // bytes written into mesh memory
  kTris,         // triangles sent to the chip (indexed meshes and cut ones)
  kCutMeshes,    // meshes that had to be cut at the camera
  kOutside,      // meshes left out because they are wholly outside the picture
  kCutReused,    // cut meshes whose cut from the frame before was still good (the camera had not moved)
  kCounters
};

extern double g_ticks[kSlots];
extern double g_count[kCounters];

inline double now() { return (double)SDL_GetPerformanceCounter(); }
inline void count(Counter c, double n) { g_count[c] += n; }

struct Span {
  Slot slot;
  double start;
  explicit Span(Slot s) : slot(s), start(now()) {}
  ~Span() { g_ticks[slot] += now() - start; }
};

// "name ms" pairs per frame (hundredths, no floating point in printf: nxdk's has no %f), then the counters per frame; resets.
// `frames` = how many frames were accumulated.
void format(char* out, int size, int frames);

}  // namespace prof
}  // namespace ote
