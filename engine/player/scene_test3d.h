// A built-in scene that exercises the 3D drawing layer (docs/RENDER3D.md): a
// small block map with water, standing pictures and a solid building, seen by
// a camera that turns in quarter steps. Not part of any game; started with
// --test-3d. It is how a new backend or platform port is checked by eye.
#pragma once
#include "gfx3d/render3d.h"
#include "scene.h"

namespace ote {

class Test3DScene : public Scene {
 public:
  explicit Test3DScene(SceneHost& host);
  ~Test3DScene() override;
  void onInput(const InputEvent& e) override;
  void update(double dt) override;
  void draw(Renderer& r) override;

 private:
  void build();

  SceneHost& host_;
  Renderer3D& r3d_;
  MeshId ground_ = 0, water_ = 0, building_ = 0, figure_ = 0;
  Texture3DId groundTex_ = 0, figureTex_ = 0;
  int turn_ = 0;            // quarter turns asked for
  float angle_ = 0;         // where the camera is now, in quarter turns
  float zoom_ = 1;
  bool perspective_ = false;
  double time_ = 0;
};

}  // namespace ote
