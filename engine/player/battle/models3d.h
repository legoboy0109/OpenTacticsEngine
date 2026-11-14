// 3D models of a game's units and structures in the 3D kinds of view
// (docs/GRAPHICS.md "Models"). A ModelAsset is one loaded file, shared by
// everything that uses it; a ModelActor is one thing on the map showing it,
// with its own place in an animation.
#pragma once
#include <string>
#include <vector>

#include "gfx3d/model.h"

namespace ote {

struct ModelAsset {
  std::string file;
  bool ok = false;
  Model3D model;
  std::vector<Texture3DId> textures;  // per material, 0 = none
  struct Piece { int node, part; MeshId rigid; };  // rigid 0 = bends with the skeleton
  std::vector<Piece> pieces;
  float height = 1, width = 1;  // rest-pose size in the model's own units (above its origin; widest side)
  bool bends = false;           // has pieces that need their own mesh per actor
};

// The models of the loaded game, by file. Loading failures are logged once
// and the thing keeps its placeholder look.
class Models3D {
 public:
  void init(const std::string& gameDir) { dir_ = gameDir; }
  const ModelAsset* get(Renderer3D& r3d, const std::string& file);
  void release(Renderer3D& r3d);

 private:
  std::string dir_;
  std::vector<ModelAsset*> assets_;
};

struct ModelActor {
  const ModelAsset* asset = nullptr;
  std::vector<MeshId> meshes;  // per piece; 0 for rigid pieces
  ModelPose pose;
  MeshData posed;              // scratch for skinning
  // What Map3D keeps per unit to pick animations and facing.
  int state = 0;
  float time = 0, facing = 0, lastX = 0, lastY = 0, lastTime = -1;
  bool placed = false;

  void release(Renderer3D& r3d);
};

struct ModelTint {
  Rgba3D side = Rgba3D(1, 1, 1);  // colour of the side material
  std::string sideMaterial;       // its name ("" = none)
  float dim = 1;                  // multiplies every colour (a unit that has acted)
  float alpha = 1;
  bool depthTest = true;
};

// Puts the actor in `clip` at `time` (clip < 0: rest pose) and re-poses its
// bending pieces. Once per frame, before drawModel.
void poseActor(Renderer3D& r3d, const ModelAsset& asset, ModelActor& actor, int clip, float time, bool loop);
// Draws the actor as last posed. May be called more than once per frame.
void drawModel(Renderer3D& r3d, const ModelAsset& asset, const ModelActor& actor, const Mat4& place, const ModelTint& tint);

}  // namespace ote
