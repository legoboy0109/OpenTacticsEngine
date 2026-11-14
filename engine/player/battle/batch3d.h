// Collects the small things that change every frame (highlights, units,
// frames) into as few meshes as possible: one per material. The meshes are
// kept and refilled, not made anew each frame.
#pragma once
#include <vector>

#include "gfx3d/render3d.h"
#include "gfx3d/shapes.h"

namespace ote {

class Batch3D {
 public:
  // The mesh to add shapes to for this material this frame.
  MeshData& mesh(const Material3D& material);
  // Uploads and draws everything collected, in the order the materials were
  // first asked for, and empties the batches.
  void flush(Renderer3D& r3d);
  void release(Renderer3D& r3d);

 private:
  struct Slot { Material3D material; MeshData data; MeshId id = 0; bool used = false; };
  std::vector<Slot> slots_;
  std::vector<size_t> order_;  // slots used this frame, in order
};

}  // namespace ote
