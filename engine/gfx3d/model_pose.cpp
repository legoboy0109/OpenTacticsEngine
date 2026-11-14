// Posing and skinning of loaded models (model.h). Placeholder bodies: the
// rest pose only, until animation is implemented.
#include "gfx3d/model.h"

namespace ote {

void poseModel(const Model3D& model, int, float, bool, ModelPose& out) {
  out.world.resize(model.nodes.size());
  for (size_t i = 0; i < model.nodes.size(); ++i) out.world[i] = model.nodeMatrix((int)i);
}

void skinPart(const Model3D& model, const ModelPose&, int, int part, MeshData& out) { out = model.parts[(size_t)part].mesh; }

}  // namespace ote
