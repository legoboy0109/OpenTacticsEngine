// Whole-device-pixel placement of the 2D layer's rectangles, for backends whose window is not a whole multiple of the
// game's logical size (a resized desktop window, a browser canvas at 125% or in an iframe). No SDL in here, so it is
// unit-tested (tests/test_pixel_snap.cpp).
//
// The problem: the logical frame is scaled by a fractional factor k, so the edges of a rectangle land between device pixels,
// and with k = 1.5 (or an odd letterbox offset) exactly ON a pixel centre. A pixel centre on a quad edge is a tie in the
// rasterizer, and the texture coordinate there is exactly the cell's edge: the nearest-filter lookup then reads rounding noise
// from the NEIGHBOURING cell of the sheet. On a sparse tileset that neighbour is transparent, and the dark clear colour shows
// as a one-pixel line along the squares (found with tests/fixtures/tiles-sparse, tools/webtest/suites/tiles.mjs).
//
// The fix: put both edges of every rectangle on whole device pixels (the edges are rounded separately, so neighbouring
// rectangles share exactly the same edge: no gap, no overlap) and hand the backend the logical coordinates that scale back to
// those edges. A pixel centre is then always half a pixel from every edge. With a whole-number scale and a whole-number offset
// nothing moves, so integer scales stay pixel-identical.
#pragma once
#include <cmath>

namespace ote {

// The mapping from logical coordinates to output pixels, per axis: device = logical * scale + offset. The two scales can
// differ slightly (SDL3 floors the letterboxed picture's size to whole pixels on each axis), and the offsets are the ones
// the backend really applies (SDL3 floors the viewport's origin), not the ideal centring.
struct OutputMap {
  float scaleX = 1, scaleY = 1;
  float offX = 0, offY = 0;
  bool known = false;  // false until the backend has asked its renderer for the mapping: rectangles then go through unchanged
};

// True when snapping would not move anything (a whole-number scale and offsets), so callers can skip the arithmetic.
inline bool outputMapIsWhole(const OutputMap& m) {
  return std::fabs(m.scaleX - std::round(m.scaleX)) < 1e-6f && std::fabs(m.scaleY - std::round(m.scaleY)) < 1e-6f &&
         std::fabs(m.offX - std::round(m.offX)) < 1e-6f &&
         std::fabs(m.offY - std::round(m.offY)) < 1e-6f;
}

// One axis: the logical interval [pos, pos + size) becomes the device interval [a, b) with whole numbers, b >= a + 1, and is
// returned as logical position / size (what the backend multiplies by `scale` again).
inline void snapSpan(float scale, float off, int pos, int size, float& outPos, float& outSize) {
  const double k = (double)scale;
  double a = std::floor((double)pos * k + (double)off + 0.5);
  double b = std::floor((double)(pos + size) * k + (double)off + 0.5);
  if (b < a + 1) b = a + 1;  // never vanishes (a downscaled 1-pixel line)
  outPos = (float)((a - (double)off) / k);
  outSize = (float)((b - a) / k);
}

// A rectangle in logical pixels to the rectangle (still in logical pixels) whose device edges are whole pixels.
inline void snapRect(const OutputMap& m, int x, int y, int w, int h, float& ox, float& oy, float& ow, float& oh) {
  if (!m.known || m.scaleX <= 0 || m.scaleY <= 0 || outputMapIsWhole(m)) {
    ox = (float)x;
    oy = (float)y;
    ow = (float)w;
    oh = (float)h;
    return;
  }
  snapSpan(m.scaleX, m.offX, x, w, ox, ow);
  snapSpan(m.scaleY, m.offY, y, h, oy, oh);
}

}  // namespace ote
