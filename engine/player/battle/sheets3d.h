// The game's art sheets as textures of the 3D drawing layer (the 2D renderer
// has its own copies in Sprites). Made the first time a sheet is asked for.
// The 3D layer wants power-of-two sides, so a sheet is padded up to the next
// one; uv() gives a sheet rectangle as texture positions in that picture.
#pragma once
#include <string>
#include <vector>

#include "core/data/graphics.h"
#include "gfx3d/render3d.h"

namespace ote {

class Sheets3D {
 public:
  void init(const std::string& gameDir, const Graphics* graphics) { dir_ = gameDir; g_ = graphics; }
  // 0 when the sheet cannot be used (logged once): missing, damaged or too large.
  Texture3DId texture(Renderer3D& r3d, int sheet);
  // Texture positions of the pixel rectangle (x, y, w, h) of a sheet whose
  // texture() is not 0, pulled in a hair so neighbours never bleed in.
  void uv(int sheet, int x, int y, int w, int h, float& u0, float& v0, float& u1, float& v1) const;
  void release(Renderer3D& r3d);

 private:
  struct Entry { bool tried = false; Texture3DId tex = 0; int w = 1, h = 1; };
  std::string dir_;
  const Graphics* g_ = nullptr;
  std::vector<Entry> sheets_;
};

}  // namespace ote
