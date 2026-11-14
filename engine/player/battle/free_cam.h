// The player's hand on the camera of the full 3D view (docs/VIEWS.md "Full
// 3D"): turning, tilting and zooming from buttons, the on-screen buttons and
// pointer drags, and following what the battle wants shown. It only moves a
// Free3DGeometry; picking and drawing read that.
#pragma once
#include "core/view/free3d.h"
#include "platform/platform.h"

namespace ote {

class FreeCam {
 public:
  // Camera buttons and drags. True = the event was one of them.
  bool input(const InputEvent& e);
  // The same from an on-screen button: one step at once, more while it is held.
  void press(Button b);
  void releaseAll();
  // Moves the camera: eases towards where the player turned it and keeps the
  // spot (X, Y, Z) of 3D space in the picture.
  void update(double dt, Free3DGeometry& g, float X, float Y, float Z);
  // The next update puts the camera where it should be at once (first frame, new screen size).
  void snap() { placed_ = false; }

 private:
  static int slot(Button b);
  float yaw_ = 0, pitch_ = 0.8726646f, distance_ = 11.0f;  // where the player wants the camera
  bool held_[6] = {false, false, false, false, false, false};
  bool placed_ = false;
};

}  // namespace ote
