// A loaded 3D model handed to the 3D layer: its meshes and textures, ready
// to draw at any place. Rest pose only for now (no animation yet).
#pragma once
#include <string>
#include <vector>

#include "gfx3d/model.h"

namespace ote {

class ModelView {
 public:
  // Loads a .glb/.gltf and uploads it. False with a plain sentence in err.
  bool load(Renderer3D& r3d, const std::string& path, std::string& err);
  void draw(Renderer3D& r3d, const Mat4& place) const;
  void release(Renderer3D& r3d);
  bool loaded() const { return !parts_.empty(); }
  const Model3D& model() const { return model_; }

 private:
  struct Part { MeshId mesh; Mat4 node; Material3D material; };
  Model3D model_;
  std::vector<Part> parts_;
  std::vector<Texture3DId> textures_;  // one per material (0 = none)
};

}  // namespace ote
