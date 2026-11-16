#include "core/view/hex_iso.h"

#include <algorithm>
#include <cmath>

namespace ote {
namespace {

// round(num / den) for positive values, halves up.
int roundDiv(long num, long den) { return (int)((2 * num + den) / (2 * den)); }

// The multiple of 4 nearest to num / den (at least 4).
int nearest4(long num, long den) { return std::max(4, 4 * roundDiv(num, 4 * den)); }

// HexGeo::rowSpan tiles exactly only if, whenever the slanted edge's number of rows (`rows`) is odd, its rise
// (`rise`) is even (the middle row of an odd count is half the rise). `longSide` (a multiple of 4) is the exact value
// rounded; when it would not tile, take the multiple of 4 on either side that is nearer to the exact value.
int tilingLongSide(int longSide, double exact, bool pointy, int shortSide) {
  auto ok = [&](int l) {
    int rows = pointy ? l / 4 : shortSide / 2, rise = pointy ? shortSide / 2 : l / 4;
    return rows % 2 == 0 || rise % 2 == 0;
  };
  if (ok(longSide)) return longSide;
  int lo = std::max(4, longSide - 4), hi = longSide + 4;
  if (!ok(lo)) return hi;
  if (!ok(hi)) return lo;
  return std::fabs(lo - exact) <= std::fabs(hi - exact) ? lo : hi;
}

}  // namespace

HexIsoGeometry HexIsoGeometry::make(const Grid& g, int tile, int tileWidth, int stepPx) {
  HexIsoGeometry h;
  h.td = HexGeo::of(g, tile);
  h.tileW = tileWidth;
  h.step = stepPx;
  const int L = h.td.L;
  if (h.td.pointy) {
    // sx = W / T, sy = sx / 2: a box T x L becomes W x (L * W / (2 T)).
    h.sq = HexGeo::ofBox(g, tileWidth, tilingLongSide(nearest4((long)L * tileWidth, 2L * tile),
                                                      (double)L * tileWidth / (2.0 * tile), true, tileWidth));
  } else {
    // A box L x T becomes (L * W / T) x (W / 2): the height is the short side and must be even.
    int shortSide = tileWidth / 2 + (tileWidth / 2) % 2;
    h.sq = HexGeo::ofBox(g, shortSide, tilingLongSide(nearest4((long)L * tileWidth, tile),
                                                      (double)L * tileWidth / tile, false, shortSide));
  }
  h.headroom = std::max(h.sq.boxH(), tileWidth);  // room for a unit (unitWidth <= ~tileWidth / 2) or a tall picture
  return h;
}

void HexIsoGeometry::topBox(int x, int y, int elevation, int& wx, int& wy) const {
  sq.box(x, y, wx, wy);
  wy += (levels - elevation) * step + headroom;
}

void HexIsoGeometry::project(float fx, float fy, float elevation, float& wx, float& wy) const {
  // Exact on the middles of the cells (see the header); fractions in between are the same straight scaling.
  wx = fx * (float)sq.boxW() / (float)td.boxW();
  wy = fy * (float)sq.boxH() / (float)td.boxH() - elevation * step + (float)(levels * step + headroom);
}

void HexIsoGeometry::unproject(float wx, float wy, float elevation, float& fx, float& fy) const {
  fx = wx * (float)td.boxW() / (float)sq.boxW();
  fy = (wy - (float)(levels * step + headroom) + elevation * step) * (float)td.boxH() / (float)sq.boxH();
}

void HexIsoGeometry::faceSpan(int dir, int& l, int& r) const {
  const int w = boxW();
  if (sq.pointy) {  // direction 3 (lower left) is the left half of the lower edge, 2 the right half
    l = dir == 3 ? 0 : w / 2;
    r = dir == 3 ? w / 2 : w;
  } else {  // 4 lower left (the first quarter), 3 the flat bottom (the middle half), 2 lower right
    const int q = sq.quarter();
    l = dir == 4 ? 0 : dir == 3 ? q : w - q;
    r = dir == 4 ? q : dir == 3 ? w - q : w;
  }
}

int HexIsoGeometry::pick(float wx, float wy, const std::vector<int>& elev) const {
  int best = -1, bestKey = -1, bestX = -1;
  // A hexagon at elevation e fills the screen column of its top face for every level 0..e (its side faces are
  // the top face swept down), so test each level's plane.
  for (int level = 0; level <= levels; ++level) {
    float fx, fy;
    unproject(wx, wy, (float)level, fx, fy);
    int x, y;
    td.nearestXY(fx, fy, x, y);
    if (x < 0 || y < 0 || x >= td.cols || y >= td.rows) continue;
    if (elev[(size_t)(y * td.cols + x)] < level) continue;
    int key = sortKey(x, y);
    if (key > bestKey || (key == bestKey && x > bestX)) {
      bestKey = key;
      bestX = x;
      best = y * td.cols + x;
    }
  }
  return best;
}

bool HexIsoGeometry::hidden(int x, int y, const std::vector<int>& elev) const {
  if (step <= 0) return false;
  // The pixel at the middle of the top face is the one a pointer there would pick: if that is another hexagon, a
  // nearer one rises over this one. (Only higher hexagons can: planes below this one's level lie further up the
  // screen, where only hexagons behind are found, and those lose to this one in pick().)
  int bx, by;
  topBox(x, y, elev[(size_t)(y * td.cols + x)], bx, by);
  return pick((float)(bx + boxW() / 2) + 0.5f, (float)(by + boxH() / 2) + 0.5f, elev) != y * td.cols + x;
}

}  // namespace ote
