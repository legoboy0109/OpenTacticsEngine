// Puts the 3D layer's picture on the screen. Backends that draw into memory
// (the software one) hand over pixels, which travel through one 2D texture;
// backends that draw straight to the screen need nothing here.
//
// Also decides how big the 3D picture is (docs/RENDER3D.md "Resolution"): the
// game's logical size, or the display's real resolution, so detailed art is
// not shrunk to 480 x 270 and scaled up again.
#pragma once
#include <string>

#include "gfx3d/render3d.h"
#include "platform/platform.h"

namespace ote {

class Show3D {
 public:
  // The "3D resolution" choice: "display", "game", or "" = automatic (the
  // display's resolution for a backend that is fast enough, else the game's).
  void setResolution(const std::string& mode) { mode_ = mode; }
  // Fills the size of the picture to draw (renderWidth / renderHeight /
  // samples) in a frame whose x, y, width, height are set. Call every frame:
  // the display may have been resized.
  void fit(const Renderer& r, const Renderer3D& r3d, Frame3D& f) const;
  // Call after Renderer3D::end(), before drawing the 2D things that lie on
  // top. The rectangle is the one given in the Frame3D, in logical pixels.
  // A picture bigger than the rectangle reaches the display at full detail
  // (smooth scaling, never doubled pixels).
  void show(Renderer& r, const Renderer3D& r3d, int x, int y, int w, int h);
  // Frees the texture (before the 2D renderer goes away or changes).
  void release(Renderer& r);

  static const int kMaxSide = 4096;  // the longest side of a 3D picture, whatever the display

 private:
  std::string mode_;
  TextureId tex_ = 0;
  int w_ = 0, h_ = 0;
  bool smooth_ = false;
};

}  // namespace ote
