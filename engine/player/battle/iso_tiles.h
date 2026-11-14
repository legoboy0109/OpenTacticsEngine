// Pictures the isometric view makes for itself: placeholder blocks for things
// without art (a top face in the thing's colour over shaded side faces), and
// plain diamonds (filled = highlight tint, outline = cursor and owner frame).
// Made once per size and colour, then drawn like any other texture.
#pragma once
#include <map>
#include <vector>

#include "core/view/iso.h"
#include "platform/platform.h"

namespace ote {

struct IsoPicture {
  TextureId tex = 0;
  int w = 0, h = 0;
};

class IsoTiles {
 public:
  // A block whose top face covers wCells x hCells squares, with side faces
  // `sideH` pixels high below it. Size: (w+h)*tileW/2 by (w+h)*tileH/2 + sideH.
  IsoPicture block(Renderer& r, const IsoGeometry& g, int wCells, int hCells, int sideH, uint32_t rgb);
  // A white diamond over wCells x hCells squares (tint it with the draw colour):
  // filled, or only a 2-pixel outline.
  IsoPicture diamond(Renderer& r, const IsoGeometry& g, int wCells, int hCells, bool outline);
  void release(Renderer& r);

 private:
  std::map<std::vector<int>, IsoPicture> cache_;
};

}  // namespace ote
