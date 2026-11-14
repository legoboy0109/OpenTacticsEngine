// Helpers that add simple shapes to a mesh (map blocks, standing pictures).
#pragma once
#include "gfx3d/render3d.h"

namespace ote {

struct Tint3D {
  uint8_t r = 255, g = 255, b = 255, a = 255;
  Tint3D() = default;
  Tint3D(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255) : r(r_), g(g_), b(b_), a(a_) {}
};

// A flat four-cornered face. Corners counter-clockwise seen from the front,
// starting bottom-left; the texture rectangle (u0,v0)-(u1,v1) is laid on it
// with (u0,v0) at the top-left corner. The normal comes from the corners.
void addQuad(MeshData& m, Vec3 bottomLeft, Vec3 bottomRight, Vec3 topRight, Vec3 topLeft, Tint3D tint = Tint3D(),
             float u0 = 0, float v0 = 0, float u1 = 1, float v1 = 1);

// A box from `lo` to `hi`. The top takes `top`, the four sides `side`; the
// underside is left out (map blocks stand on something). Each face shows the
// whole texture; sides repeat it once per unit of height.
void addBlock(MeshData& m, Vec3 lo, Vec3 hi, Tint3D top, Tint3D side);

}  // namespace ote
