// Cutting triangles at the camera for the original Xbox's chip (docs/RENDER3D.md "Original Xbox backend", "Clipping").
//
// The chip draws what the vertex program gives it after the divide by w. A vertex behind the camera (w <= 0) comes out
// mirrored and a vertex very close to the camera lands thousands of pixels outside the screen, beyond what the chip's
// rasteriser handles. So a mesh that is not entirely inside the clip volume (near plane, and a guard band around the
// screen's edges) is cut by the CPU first: triangles that cross the volume's boundary are cut and come out as loose
// triangles in model space, whole triangles keep their indices, triangles entirely outside are dropped. Plain C++ with
// no console headers (tests/test_nxdk_hw3d.cpp).
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include "gfx3d/render3d.h"

namespace ote {
namespace hw3d {

enum class Visibility { Inside, Straddles, Outside };

// `clip` = four rows of projection * view * model, as the vertex program takes them (clip.x = row0 . (x, y, z, 1) ...).
// `guard`: how far past the screen's edge a vertex may be, in half screens (2 = twice as far as the edge).
// Where the mesh's bounding box (lo..hi in model space) lies relative to the volume
//    z + w >= 0  (in front of the camera),  |x| <= guard * w,  |y| <= guard * w.
Visibility classifyBox(const float clip[16], const float lo[3], const float hi[3], float guard);

struct ClipResult {
  std::vector<uint16_t> keep;      // indices of whole triangles that are entirely inside
  std::vector<Vertex3D> loose;     // cut triangles: three vertices each, model space, same winding
};

// Splits a mesh's triangles by the same volume. Appends to `out` (which the caller clears).
void clipMesh(const Vertex3D* vertices, size_t vertexCount, const uint16_t* indices, size_t indexCount, const float clip[16],
              float guard, ClipResult& out);

}  // namespace hw3d
}  // namespace ote
