// A block of the isometric view whose top face shows a picture (a map tile, docs/GRAPHICS.md "Tile layer") laid flat on the diamond.
// The shape and shading are those of IsoTiles::block (iso_tiles.cpp); the top face samples the picture instead of one colour.
#include <cmath>

#include "battle/iso_tiles.h"

namespace ote {

IsoPicture IsoTiles::pictureBlock(Renderer& r, const IsoGeometry& g, int sideH, uint32_t rgb, const std::vector<int>& id, const uint8_t* rgba,
                                  int imgW, int sx, int sy, int sw, int sh) {
  std::vector<int> key{1, g.tileW, g.tileH, sideH, (int)rgb};
  key.insert(key.end(), id.begin(), id.end());
  auto it = cache_.find(key);
  if (it != cache_.end()) return it->second;
  const float hw = g.tileW * 0.5f, hh = g.tileH * 0.5f;
  IsoPicture pic;
  pic.w = g.tileW;
  pic.h = g.tileH + sideH;
  std::vector<uint8_t> px((size_t)pic.w * pic.h * 4, 0);
  // Is the middle of pixel (x, y) inside the diamond; if so, where in the square (0..1 each way, x to the lower right, y to the lower left).
  auto face = [&](int x, int y, float& gx, float& gy) {
    const float u = (x + 0.5f - hw) / hw, v = (y + 0.5f) / hh;
    gx = (u + v) * 0.5f;
    gy = (v - u) * 0.5f;
    return gx >= 0 && gy >= 0 && gx < 1 && gy < 1;
  };
  auto put = [&](int x, int y, int cr, int cg, int cb, float k) {
    uint8_t* p = &px[((size_t)y * pic.w + (size_t)x) * 4];
    p[0] = (uint8_t)(cr * k);
    p[1] = (uint8_t)(cg * k);
    p[2] = (uint8_t)(cb * k);
    p[3] = 255;
  };
  const int br = (int)((rgb >> 16) & 255), bg = (int)((rgb >> 8) & 255), bb = (int)(rgb & 255);
  const int frontX = g.tileW / 2;
  for (int x = 0; x < pic.w; ++x) {
    int last = -1;
    float gx, gy;
    for (int y = 0; y < g.tileH; ++y)
      if (face(x, y, gx, gy)) last = y;
    if (last < 0) continue;
    for (int y = 0; y <= last; ++y) {
      if (!face(x, y, gx, gy)) continue;
      const int tx = sx + std::min(sw - 1, (int)(gx * sw)), ty = sy + std::min(sh - 1, (int)(gy * sh));
      const uint8_t* s = &rgba[((size_t)ty * (size_t)imgW + (size_t)tx) * 4];
      const int a = s[3];
      // The picture over the block's colour (a transparent pixel shows the colour, like a terrain picture does).
      put(x, y, (s[0] * a + br * (255 - a)) / 255, (s[1] * a + bg * (255 - a)) / 255, (s[2] * a + bb * (255 - a)) / 255, y == last ? 0.86f : 1.0f);
    }
    const float shade = x < frontX ? 0.70f : 0.52f;
    for (int y = last + 1; y <= last + sideH && y < pic.h; ++y) put(x, y, br, bg, bb, y == last + sideH ? shade * 0.8f : shade);
  }
  pic.tex = r.createTexture(px.data(), pic.w, pic.h);
  cache_[key] = pic;
  return pic;
}

}  // namespace ote
