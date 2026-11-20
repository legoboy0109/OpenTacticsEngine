#include "battle/facing_marker.h"

#include <algorithm>
#include <cmath>

#include "ui/text.h"

namespace ote {

namespace {

// Unit vectors (y down, neighbour spacing 1) of the directions of each grid, in direction order (grid/facing.h): squares N E S W; pointy hexagons
// NE E SE SW W NW; flat hexagons N NE SE S SW NW.
const float kS = 0.8660254f;
const float kSquareDir[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
const float kPointyDir[6][2] = {{0.5f, -kS}, {1, 0}, {0.5f, kS}, {-0.5f, kS}, {-1, 0}, {-0.5f, -kS}};
const float kFlatDir[6][2] = {{0, -1}, {kS, -0.5f}, {kS, 0.5f}, {0, 1}, {-kS, 0.5f}, {-kS, -0.5f}};

// Is the point inside the triangle (a, b, c), with every edge pushed out by `rim` pixels? (Edge functions normalised to pixels.)
bool inside(const float (&p)[3][2], float x, float y, float rim) {
  float sign = 0;
  for (int i = 0; i < 3; ++i) {
    const float ax = p[i][0], ay = p[i][1], bx = p[(i + 1) % 3][0], by = p[(i + 1) % 3][1];
    const float len = std::max(1e-3f, std::hypot(bx - ax, by - ay));
    const float cross = ((bx - ax) * (y - ay) - (by - ay) * (x - ax)) / len;  // signed distance to the edge's line
    if (i == 0) sign = cross >= 0 ? 1.f : -1.f;
    if (cross * sign < -rim) return false;
  }
  return true;
}

}  // namespace

void facingEdgeVector(const Grid& g, int dir, int tile, float& fx, float& fy) {
  fx = fy = 0;
  if (dir < 0 || dir >= g.directionCount()) return;
  const float(*v)[2] = !g.isHex() ? kSquareDir : g.layout() == HexLayout::Pointy ? kPointyDir : kFlatDir;
  fx = v[dir][0] * tile * 0.5f;
  fy = v[dir][1] * tile * 0.5f;
}

int shownFacing(const DrawCtx& c, const Unit& u) {
  if (!c.battle.facingGame()) return -1;
  if (u.id >= 0 && (size_t)u.id < c.view.units.size() && c.view.units[(size_t)u.id].facing >= 0) return c.view.units[(size_t)u.id].facing;
  return u.facing;
}

void drawFacingMarker(Renderer& r, const DrawCtx& c, const Unit& u, int cx, int cy, int ex, int ey, float alpha) {
  if (shownFacing(c, u) < 0) return;
  const float vx = (float)(ex - cx), vy = (float)(ey - cy);
  const float len = std::hypot(vx, vy);
  if (len < 3.0f) return;
  const float ux = vx / len, uy = vy / len;                        // along the direction
  const float px = -uy, py = ux;                                   // across it
  const float half = std::max(2.0f, len * 0.34f);                  // half the width of the arrow's base
  const float tipAt = len * 0.98f, baseAt = std::max(len * 0.55f, tipAt - half * 1.7f);
  const float pts[3][2] = {{cx + ux * tipAt, cy + uy * tipAt},
                           {cx + ux * baseAt + px * half, cy + uy * baseAt + py * half},
                           {cx + ux * baseAt - px * half, cy + uy * baseAt - py * half}};
  float lo[2] = {pts[0][0], pts[0][1]}, hi[2] = {pts[0][0], pts[0][1]};
  for (int i = 1; i < 3; ++i)
    for (int k = 0; k < 2; ++k) {
      lo[k] = std::min(lo[k], pts[i][k]);
      hi[k] = std::max(hi[k], pts[i][k]);
    }
  const Color rim = withAlpha(Color(10, 10, 20, 230), (int)(230 * alpha)), fill = withAlpha(Color(255, 236, 140, 255), (int)(255 * alpha));
  float tri[3][2];
  for (int i = 0; i < 3; ++i) {
    tri[i][0] = pts[i][0];
    tri[i][1] = pts[i][1];
  }
  for (int pass = 0; pass < 2; ++pass)  // the dark rim first, the light arrowhead over it
    for (int y = (int)std::floor(lo[1]) - 1; y <= (int)std::ceil(hi[1]) + 1; ++y)
      for (int x = (int)std::floor(lo[0]) - 1; x <= (int)std::ceil(hi[0]) + 1; ++x)
        if (inside(tri, x + 0.5f, y + 0.5f, pass == 0 ? 1.0f : 0.0f)) r.fillRect(x, y, 1, 1, pass == 0 ? rim : fill);
}

void drawFacingMarkerFlat(Renderer& r, const DrawCtx& c, const Unit& u, int cx, int cy, int tile, float alpha) {
  const int dir = shownFacing(c, u);
  if (dir < 0) return;
  float fx, fy;
  facingEdgeVector(c.battle.grid, dir, tile, fx, fy);
  drawFacingMarker(r, c, u, cx, cy, cx + (int)std::lround(fx), cy + (int)std::lround(fy), alpha);
}

}  // namespace ote
