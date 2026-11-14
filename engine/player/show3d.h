// Puts the 3D layer's picture on the screen. Backends that draw into memory
// (the software one) hand over pixels, which travel through one 2D texture;
// backends that draw straight to the screen need nothing here.
#pragma once
#include "gfx3d/render3d.h"
#include "platform/platform.h"

namespace ote {

class Show3D {
 public:
  // Call after Renderer3D::end(), before drawing the 2D things that lie on
  // top. The rectangle is the one given in the Frame3D, in logical pixels.
  void show(Renderer& r, const Renderer3D& r3d, int x, int y, int w, int h);
  // Frees the texture (before the 2D renderer goes away or changes).
  void release(Renderer& r);

 private:
  TextureId tex_ = 0;
  int w_ = 0, h_ = 0;
};

}  // namespace ote
