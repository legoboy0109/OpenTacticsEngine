// The placeholder block of a hexagon in the 2D isometric view (docs/HEX.md "2D isometric"): the colour on the top
// face, darker side faces under the lower edges. Made once per size and colour like the square blocks.
#include <algorithm>

#include "battle/iso_tiles.h"

namespace ote {

IsoPicture IsoTiles::hexBlock(Renderer& r, const HexGeo& sq, int sideH, uint32_t rgb) {
  std::vector<int>key{1, sq.boxW(), sq.boxH(), sq.pointy ? 1 : 0, sideH, (int)rgb};
  auto it = cache_.find(key);
  if (it != cache_.end()) return it->second;
  IsoPicture pic;
  pic.w = sq.boxW();
  pic.h = sq.boxH() + sideH;
  std::vector<uint8_t> px((size_t)pic.w * pic.h * 4, 0);
  auto put = [&](int x, int y, float k) {
    uint8_t* p = &px[(size_t)(y * pic.w + x) * 4];
    p[0] = (uint8_t)(((rgb >> 16) & 255) * k);
    p[1] = (uint8_t)(((rgb >> 8) & 255) * k);
    p[2] = (uint8_t)((rgb & 255) * k);
    p[3] = 255;
  };
  const int q = sq.quarter();
  // Shade of the face under column x: lit on the left, dark on the right (a flat hexagon's front face between).
  auto faceShade = [&](int x) {
    if (sq.pointy) return x < pic.w / 2 ? 0.70f : 0.52f;
    return x < q ? 0.70f : x < pic.w - q ? 0.61f : 0.52f;
  };
  std::vector<int> last((size_t)pic.w, -1);  // lowest row of the top face in each column
  for (int row = 0; row < sq.boxH(); ++row) {
    int l, rr;
    sq.rowSpan(row, l, rr);
    for (int x = l; x < rr; ++x) last[(size_t)x] = row;
  }
  for (int row = 0; row < sq.boxH(); ++row) {
    int l, rr;
    sq.rowSpan(row, l, rr);
    for (int x = l; x < rr; ++x) put(x, row, row == last[(size_t)x] ? 0.86f : 1.0f);  // the lower rim a shade darker
  }
  for (int x = 0; x < pic.w; ++x) {
    if (last[(size_t)x] < 0) continue;
    float shade = faceShade(x);
    for (int y = last[(size_t)x] + 1; y <= last[(size_t)x] + sideH && y < pic.h; ++y)
      put(x, y, y == last[(size_t)x] + sideH ? shade * 0.8f : shade);
  }
  pic.tex = r.createTexture(px.data(), pic.w, pic.h);
  cache_[key] = pic;
  return pic;
}

}  // namespace ote
