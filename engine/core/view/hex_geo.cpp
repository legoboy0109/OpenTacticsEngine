#include "core/view/hex_geo.h"

#include <algorithm>
#include <cmath>

namespace ote {
namespace {

// A ramp of `q` integer steps rising from about 0 to `h`, with ramp(j) + ramp(q-1-j) == h for every j: the
// half-width of a hexagon's slanted edge at its j-th pixel row. Because the two halves of an edge that two
// neighbouring hexagons share add up to exactly the offset between their middles, the hexagons tile exactly.
int ramp(int j, int h, int q) {
  if (q <= 0) return h;
  if (2 * j + 1 > q) return h - ramp(q - 1 - j, h, q);  // mirror the second half
  if (2 * j + 1 == q) return h / 2;                      // the middle row of an odd count
  return (h * (2 * j + 1) + q) / (2 * q);                // round(h * (j + 0.5) / q)
}

int floorDiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

}  // namespace

int HexGeo::longSide(int tile) {
  int l = (int)std::lround(tile * 1.1547005 / 4.0) * 4;  // nearest multiple of 4
  return std::max(4, l);
}

HexGeo HexGeo::of(const Grid& g, int tile) {
  HexGeo h;
  h.pointy = g.layout() == HexLayout::Pointy;
  h.T = tile;
  h.L = longSide(tile);
  h.cols = g.width();
  h.rows = g.height();
  return h;
}

HexGeo HexGeo::ofBox(const Grid& g, int shortSide, int longSide) {
  HexGeo h = of(g, shortSide);
  h.L = longSide;
  return h;
}

void HexGeo::box(int x, int y, int& bx, int& by) const {
  const int step = 3 * quarter();
  if (pointy) {
    bx = x * T + ((y & 1) ? T / 2 : 0);
    by = y * step;
  } else {
    bx = x * step;
    by = y * T + ((x & 1) ? T / 2 : 0);
  }
}

void HexGeo::middle(int x, int y, int& mx, int& my) const {
  box(x, y, mx, my);
  mx += boxW() / 2;
  my += boxH() / 2;
}

int HexGeo::mapW() const {
  if (cols <= 0) return 0;
  return pointy ? cols * T + T / 2 : (cols - 1) * 3 * quarter() + L;
}

int HexGeo::mapH() const {
  if (rows <= 0) return 0;
  return pointy ? (rows - 1) * 3 * quarter() + L : rows * T + T / 2;
}

void HexGeo::structureBox(int x, int y, int rings, int& bx, int& by, int& bw, int& bh) const {
  box(x, y, bx, by);
  bw = boxW();
  bh = boxH();
  if (rings <= 0) return;
  // The seven hexagons reach one hexagon further in every direction: left/right (pointy) or up/down (flat) a whole
  // box, the other way three quarters of the long side.
  if (pointy) {
    bx -= T;
    by -= 3 * quarter();
    bw = 3 * T;
    bh = 10 * quarter();
  } else {
    bx -= 3 * quarter();
    by -= T;
    bw = 10 * quarter();
    bh = 3 * T;
  }
}

void HexGeo::nearestXY(float fx, float fy, int& bestX, int& bestY) const {
  const int q3 = 3 * quarter();
  double best = 1e300;
  bestX = bestY = 0;
  if (pointy) {
    int r0 = floorDiv((int)std::floor(fy) - L / 2 + q3 / 2, q3);
    for (int y = r0 - 1; y <= r0 + 1; ++y) {
      int off = (y & 1) ? T / 2 : 0;
      int c0 = floorDiv((int)std::floor(fx) - T / 2 - off, T);
      for (int x = c0; x <= c0 + 1; ++x) {
        int mx, my;
        middle(x, y, mx, my);
        double d = (mx - fx) * (double)(mx - fx) + (my - fy) * (double)(my - fy);
        if (d < best) { best = d; bestX = x; bestY = y; }
      }
    }
  } else {
    int c0 = floorDiv((int)std::floor(fx) - L / 2 + q3 / 2, q3);
    for (int x = c0 - 1; x <= c0 + 1; ++x) {
      int off = (x & 1) ? T / 2 : 0;
      int r0 = floorDiv((int)std::floor(fy) - T / 2 - off, T);
      for (int y = r0; y <= r0 + 1; ++y) {
        int mx, my;
        middle(x, y, mx, my);
        double d = (mx - fx) * (double)(mx - fx) + (my - fy) * (double)(my - fy);
        if (d < best) { best = d; bestX = x; bestY = y; }
      }
    }
  }
}

Cell HexGeo::pick(float fx, float fy) const {
  int x, y;
  nearestXY(fx, fy, x, y);
  if (x < 0 || y < 0 || x >= cols || y >= rows) return kNoCell;
  return (Cell)(y * cols + x);
}

Cell HexGeo::nearestCell(float fx, float fy) const {
  // Clamp the lattice position onto the map, then look at its neighbours for the nearest middle (cheap; used for
  // points just outside the map and units mid-step).
  int x, y;
  nearestXY(fx, fy, x, y);
  x = std::max(0, std::min(x, cols - 1));
  y = std::max(0, std::min(y, rows - 1));
  double best = 1e300;
  int bx = x, by = y;
  for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx) {
      int cx = x + dx, cy = y + dy;
      if (cx < 0 || cy < 0 || cx >= cols || cy >= rows) continue;
      int mx, my;
      middle(cx, cy, mx, my);
      double d = (mx - fx) * (double)(mx - fx) + (my - fy) * (double)(my - fy);
      if (d < best) { best = d; bx = cx; by = cy; }
    }
  return (Cell)(by * cols + bx);
}

void HexGeo::rowSpan(int row, int& l, int& r) const {
  if (pointy) {
    // Corners at the top and bottom middle, upright sides from 1/4 to 3/4 of the height.
    const int q = quarter(), h = T / 2;
    int hw;
    if (row < q) hw = ramp(row, h, q);
    else if (row < 3 * q) hw = h;
    else hw = ramp(q - 1 - std::min(row - 3 * q, q - 1), h, q);
    l = T / 2 - hw;
    r = T / 2 + hw;
  } else {
    // Flat top and bottom edges (half the long side wide), the widest row in the middle.
    const int half = T / 2, h = quarter();
    int d = row < half ? half - 1 - row : std::min(row - half, half - 1);  // distance from the middle, 0 = nearest
    int hw = L / 2 - ramp(d, h, half);
    l = L / 2 - hw;
    r = L / 2 + hw;
  }
}

// ---- both kinds of grid

void cellCentre(const Grid& g, int tile, Cell c, float& fx, float& fy) {
  if (!g.isHex()) {
    fx = (g.x(c) + 0.5f) * tile;
    fy = (g.y(c) + 0.5f) * tile;
    return;
  }
  int mx, my;
  HexGeo::of(g, tile).middle(g.x(c), g.y(c), mx, my);
  fx = (float)mx;
  fy = (float)my;
}

void cellTopLeft(const Grid& g, int tile, Cell c, float& fx, float& fy) {
  if (!g.isHex()) {
    fx = (float)(g.x(c) * tile);
    fy = (float)(g.y(c) * tile);
    return;
  }
  cellCentre(g, tile, c, fx, fy);
  fx -= tile / 2.0f;
  fy -= tile / 2.0f;
}

void structureBox(const Grid& g, int tile, int x, int y, const StructureDef& def, int& bx, int& by, int& bw, int& bh) {
  if (!g.isHex()) {
    bx = x * tile;
    by = y * tile;
    bw = def.width * tile;
    bh = def.height * tile;
    return;
  }
  HexGeo::of(g, tile).structureBox(x, y, def.rings, bx, by, bw, bh);
}

Cell cellNearPos(const Grid& g, int tile, float px, float py) {
  if (!g.isHex())
    return g.at(std::max(0, std::min((int)std::lround(px / tile), g.width() - 1)),
                std::max(0, std::min((int)std::lround(py / tile), g.height() - 1)));
  return HexGeo::of(g, tile).nearestCell(px + tile / 2.0f, py + tile / 2.0f);
}

}  // namespace ote
