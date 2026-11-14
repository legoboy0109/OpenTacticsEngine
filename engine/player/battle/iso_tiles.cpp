#include "battle/iso_tiles.h"

#include <algorithm>
#include <cmath>

namespace ote {
namespace {

// Is the middle of pixel (px, py) inside the top face of a wCells x hCells
// footprint whose bounding box starts at (0, 0)?
struct Face {
  float hw, hh;
  int w, h;
  bool has(int px, int py) const {
    float u = (px + 0.5f - h * hw) / hw, v = (py + 0.5f) / hh;
    float gx = (u + v) * 0.5f, gy = (v - u) * 0.5f;
    return gx >= 0 && gy >= 0 && gx < w && gy < h;
  }
};

void put(std::vector<uint8_t>& px, int W, int x, int y, uint32_t rgb, float k, uint8_t a = 255) {
  uint8_t* p = &px[(size_t)(y * W + x) * 4];
  p[0] = (uint8_t)(((rgb >> 16) & 255) * k);
  p[1] = (uint8_t)(((rgb >> 8) & 255) * k);
  p[2] = (uint8_t)((rgb & 255) * k);
  p[3] = a;
}

}  // namespace

IsoPicture IsoTiles::block(Renderer& r, const IsoGeometry& g, int wCells, int hCells, int sideH, uint32_t rgb) {
  std::vector<int> key{0, g.tileW, g.tileH, wCells, hCells, sideH, (int)rgb};
  auto it = cache_.find(key);
  if (it != cache_.end()) return it->second;
  Face f{g.tileW * 0.5f, g.tileH * 0.5f, wCells, hCells};
  int topH = (wCells + hCells) * g.tileH / 2;
  IsoPicture pic;
  pic.w = (wCells + hCells) * g.tileW / 2;
  pic.h = topH + sideH;
  std::vector<uint8_t> px((size_t)pic.w * pic.h * 4, 0);
  int frontX = wCells * g.tileW / 2;  // the corner nearest the viewer
  for (int x = 0; x < pic.w; ++x) {
    int last = -1;
    for (int y = 0; y < topH; ++y)
      if (f.has(x, y)) last = y;
    if (last < 0) continue;
    for (int y = 0; y <= last; ++y)
      if (f.has(x, y)) put(px, pic.w, x, y, rgb, y == last ? 0.86f : 1.0f);  // near edges a shade darker: squares read apart
    float shade = x < frontX ? 0.70f : 0.52f;
    for (int y = last + 1; y <= last + sideH && y < pic.h; ++y)
      put(px, pic.w, x, y, rgb, y == last + sideH ? shade * 0.8f : shade);
  }
  pic.tex = r.createTexture(px.data(), pic.w, pic.h);
  cache_[key] = pic;
  return pic;
}

IsoPicture IsoTiles::diamond(Renderer& r, const IsoGeometry& g, int wCells, int hCells, bool outline) {
  std::vector<int> key{outline ? 2 : 1, g.tileW, g.tileH, wCells, hCells};
  auto it = cache_.find(key);
  if (it != cache_.end()) return it->second;
  Face f{g.tileW * 0.5f, g.tileH * 0.5f, wCells, hCells};
  IsoPicture pic;
  pic.w = (wCells + hCells) * g.tileW / 2;
  pic.h = (wCells + hCells) * g.tileH / 2;
  std::vector<uint8_t> px((size_t)pic.w * pic.h * 4, 0);
  for (int y = 0; y < pic.h; ++y)
    for (int x = 0; x < pic.w; ++x) {
      if (!f.has(x, y)) continue;
      bool edge = !(f.has(x - 3, y) && f.has(x + 3, y) && f.has(x, y - 2) && f.has(x, y + 2));
      if (!outline || edge) put(px, pic.w, x, y, 0xffffff, 1.0f);
    }
  pic.tex = r.createTexture(px.data(), pic.w, pic.h);
  cache_[key] = pic;
  return pic;
}

void IsoTiles::release(Renderer& r) {
  for (auto& e : cache_)
    if (e.second.tex) r.destroyTexture(e.second.tex);
  cache_.clear();
}

}  // namespace ote
