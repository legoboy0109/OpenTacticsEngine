// Isometric projection of the square grid (docs/VIEWS.md). Pure maths, shared
// by every drawer of the "isometric" kind of view and by pointer picking.
//
// Grid space: x grows to the screen's lower right, y to the lower left, in
// squares; (x, y) is the top corner of square (x, y), (x + 0.5, y + 0.5) its
// centre. Elevation is in height levels above the lowest square of the map.
// World pixels: origin at the top-left of the box around the whole map.
#pragma once
#include <vector>

namespace ote {

struct IsoGeometry {
  int tileW = 48, tileH = 24;  // the diamond of one square
  int step = 12;               // pixels one height level raises a square
  int gridW = 0, gridH = 0;
  int levels = 0;              // highest elevation on the map (>= 0)
  int headroom = 0;            // free pixels kept above the highest corner (tall art)

  void project(float gx, float gy, float elevation, float& wx, float& wy) const {
    wx = (gx - gy) * (tileW * 0.5f) + gridH * (tileW * 0.5f);
    wy = (gx + gy) * (tileH * 0.5f) - elevation * step + (float)(levels * step + headroom);
  }
  // Grid position of a world pixel, assuming it lies on a surface at `elevation`.
  void unproject(float wx, float wy, float elevation, float& gx, float& gy) const {
    float u = (wx - gridH * (tileW * 0.5f)) / (tileW * 0.5f);
    float v = (wy - (float)(levels * step + headroom) + elevation * step) / (tileH * 0.5f);
    gx = (u + v) * 0.5f;
    gy = (v - u) * 0.5f;
  }
  int worldW() const { return (gridW + gridH) * tileW / 2; }
  // Down to the bottom of the lowest squares' side faces.
  int worldH() const { return (gridW + gridH) * tileH / 2 + levels * step + step + headroom; }

  // The square under a world pixel, or -1: the one nearest the viewer whose
  // top face or side faces contain the pixel. `elev` holds one elevation per
  // square, row by row (gridW * gridH values, each 0..levels).
  int pick(float wx, float wy, const std::vector<int>& elev) const;
  // True if squares nearer the viewer rise high enough to cover the middle of
  // square (x, y)'s top face (so what stands there needs showing through).
  bool hidden(int x, int y, const std::vector<int>& elev) const;
};

}  // namespace ote
