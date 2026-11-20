#include "core/grid/facing.h"

#include <algorithm>
#include <cstdlib>

namespace ote {

namespace {

const char* const kSquareNames[4] = {"north", "east", "south", "west"};
const char* const kPointyNames[6] = {"northeast", "east", "southeast", "southwest", "west", "northwest"};
const char* const kFlatNames[6] = {"north", "northeast", "southeast", "south", "southwest", "northwest"};

// Axial unit steps (q, r) of the six hexagon directions, in direction order (docs/HEX.md; checked against Grid::neighbor by tests/test_facing_grid.cpp).
const int kPointyStep[6][2] = {{1, -1}, {1, 0}, {0, 1}, {-1, 1}, {-1, 0}, {0, -1}};
const int kFlatStep[6][2] = {{0, -1}, {1, -1}, {1, 0}, {0, 1}, {-1, 1}, {-1, 0}};

// Axial coordinates of an offset cell, as Grid does it (pointy: odd rows shoved right; flat: odd columns shoved down).
void axialOf(const Grid& g, Cell c, int& q, int& r) {
  const int x = g.x(c), y = g.y(c);
  if (g.layout() == HexLayout::Pointy) {
    q = x - (y - (y & 1)) / 2;
    r = y;
  } else {
    q = x;
    r = y - (x - (x & 1)) / 2;
  }
}

}  // namespace

std::string directionName(const Grid& g, int dir) {
  if (dir < 0 || dir >= g.directionCount()) return "";
  if (!g.isHex()) return kSquareNames[dir];
  return g.layout() == HexLayout::Pointy ? kPointyNames[dir] : kFlatNames[dir];
}

int directionFromName(const Grid& g, const std::string& name) {
  for (int d = 0; d < g.directionCount(); ++d)
    if (directionName(g, d) == name) return d;
  return -1;
}

int directionsToward(const Grid& g, Cell from, Cell to, int out[2]) {
  if (!g.valid(from) || !g.valid(to) || from == to) return 0;
  if (!g.isHex()) {
    const int dx = g.x(to) - g.x(from), dy = g.y(to) - g.y(from);
    const int vertical = dy < 0 ? 0 : 2, horizontal = dx > 0 ? 1 : 3;  // north / south, east / west
    if (std::abs(dx) > std::abs(dy)) {
      out[0] = horizontal;
      return 1;
    }
    if (std::abs(dy) > std::abs(dx)) {
      out[0] = vertical;
      return 1;
    }
    out[0] = std::min(vertical, horizontal);  // a true diagonal: both, lower number first
    out[1] = std::max(vertical, horizontal);
    return 2;
  }
  int qa, ra, qb, rb;
  axialOf(g, from, qa, ra);
  axialOf(g, to, qb, rb);
  const int dq = qb - qa, dr = rb - ra, ds = -dq - dr;
  const int(*step)[2] = g.layout() == HexLayout::Pointy ? kPointyStep : kFlatStep;
  // The cube dot product of the offset with a direction is proportional to the cosine of the angle between them: the largest wins, a tie is a
  // line exactly between two directions.
  int best = -1000000, count = 0;
  for (int d = 0; d < 6; ++d) {
    const int uq = step[d][0], ur = step[d][1], us = -uq - ur;
    const int dot = dq * uq + dr * ur + ds * us;
    if (dot > best) {
      best = dot;
      count = 0;
    }
    if (dot == best && count < 2) out[count++] = d;
  }
  return count;
}

int directionToward(const Grid& g, Cell from, Cell to) {
  int dirs[2];
  return directionsToward(g, from, to, dirs) > 0 ? dirs[0] : -1;
}

int arcOfDirection(const Grid& g, int facing, int dir) {
  const int n = g.directionCount();
  if (facing < 0 || dir < 0 || n == 0) return kArcFront;
  const int rel = ((dir - facing) % n + n) % n;
  if (rel == 0) return kArcFront;
  if (!g.isHex()) return rel == n / 2 ? kArcBack : kArcSide;
  if (rel == 1 || rel == n - 1) return kArcFront;  // the two directions next to the facing are still ahead
  return rel == n / 2 ? kArcBack : kArcSide;
}

int arcOf(const Grid& g, Cell defender, int facing, Cell attacker) {
  int dirs[2];
  const int count = directionsToward(g, defender, attacker, dirs);
  int arc = kArcBack;
  for (int i = 0; i < count; ++i) arc = std::min(arc, arcOfDirection(g, facing, dirs[i]));
  return count == 0 ? kArcFront : arc;
}

}  // namespace ote
