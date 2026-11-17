#include "core/grid/grid.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace ote {

namespace {

// Hexagon neighbour offsets (dx, dy) per direction; [parity of the row (pointy) or column (flat)].
// This is the contract of docs/HEX.md: the order and the offsets must not change.
const int kPointy[2][6][2] = {
    {{0, -1}, {1, 0}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}},   // even row
    {{1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 0}, {0, -1}}};    // odd row
const int kFlat[2][6][2] = {
    {{0, -1}, {1, -1}, {1, 0}, {0, 1}, {-1, 0}, {-1, -1}},   // even column
    {{0, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}}};    // odd column

const int kSquare[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};

// Axial coordinates (q, r) of an offset cell; cube distance is (|dq| + |dr| + |dq + dr|) / 2.
void axial(HexLayout l, int x, int y, int& q, int& r) {
  if (l == HexLayout::Pointy) {  // odd rows shoved right
    q = x - (y - (y & 1)) / 2;
    r = y;
  } else {  // flat: odd columns shoved down
    q = x;
    r = y - (x - (x & 1)) / 2;
  }
}

}  // namespace

Grid::Grid(GridType type, int width, int height, HexLayout layout)
    : type_(type), layout_(layout), width_(width), height_(height) {}

Cell Grid::at(int x, int y) const {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) return kNoCell;
  return y * width_ + x;
}

Cell Grid::neighbor(Cell c, int dir) const {
  if (!valid(c) || dir < 0 || dir >= directionCount()) return kNoCell;
  int cx = x(c), cy = y(c);
  if (!isHex()) return at(cx + kSquare[dir][0], cy + kSquare[dir][1]);
  const int(*d)[2] = layout_ == HexLayout::Pointy ? kPointy[cy & 1] : kFlat[cx & 1];
  return at(cx + d[dir][0], cy + d[dir][1]);
}

int Grid::neighbors(Cell c, Cell out[kMaxNeighbors]) const {
  int n = 0;
  for (int i = 0; i < directionCount(); ++i) {
    Cell nb = neighbor(c, i);
    if (nb != kNoCell) out[n++] = nb;
  }
  return n;
}

int Grid::directionTo(Cell from, Cell to) const {
  for (int i = 0; i < directionCount(); ++i)
    if (neighbor(from, i) == to && to != kNoCell) return i;
  return -1;
}

int Grid::distance(Cell a, Cell b) const {
  if (!isHex()) return std::abs(x(a) - x(b)) + std::abs(y(a) - y(b));
  int qa, ra, qb, rb;
  axial(layout_, x(a), y(a), qa, ra);
  axial(layout_, x(b), y(b), qb, rb);
  int dq = qa - qb, dr = ra - rb;
  return (std::abs(dq) + std::abs(dr) + std::abs(dq + dr)) / 2;
}

void Grid::within(Cell c, int radius, std::vector<Cell>& out) const {
  out.clear();
  if (!valid(c) || radius < 0) return;
  int cx = x(c), cy = y(c);
  if (!isHex()) {
    int y0 = cy - radius < 0 ? 0 : cy - radius, y1 = cy + radius >= height_ ? height_ - 1 : cy + radius;
    for (int yy = y0; yy <= y1; ++yy) {
      int span = radius - std::abs(yy - cy);
      int x0 = cx - span < 0 ? 0 : cx - span, x1 = cx + span >= width_ ? width_ - 1 : cx + span;
      for (int xx = x0; xx <= x1; ++xx) out.push_back(yy * width_ + xx);
    }
    return;
  }
  // Hexagons: the bounding box (one extra for the half-step shift), filtered by distance.
  int y0 = cy - radius - 1 < 0 ? 0 : cy - radius - 1, y1 = cy + radius + 1 >= height_ ? height_ - 1 : cy + radius + 1;
  int x0 = cx - radius - 1 < 0 ? 0 : cx - radius - 1, x1 = cx + radius + 1 >= width_ ? width_ - 1 : cx + radius + 1;
  for (int yy = y0; yy <= y1; ++yy)
    for (int xx = x0; xx <= x1; ++xx) {
      Cell cell = yy * width_ + xx;
      if (distance(c, cell) <= radius) out.push_back(cell);
    }
}

void Grid::line(Cell a, Cell b, std::vector<Cell>& out) const {
  out.clear();
  if (!valid(a) || !valid(b)) return;
  if (a == b) {
    out.push_back(a);
    return;
  }
  if (!isHex()) {
    const double x0 = x(a), y0 = y(a), dx = x(b) - x0, dy = y(b) - y0;
    const int steps = (int)std::max(std::fabs(dx), std::fabs(dy));
    for (int i = 0; i <= steps; ++i) {
      const double t = (double)i / steps;
      const int cx = (int)std::floor(x0 + dx * t + 0.5 + 1e-9), cy = (int)std::floor(y0 + dy * t + 0.5 + 1e-9);
      out.push_back(at(cx, cy));
    }
    return;
  }
  const int n = distance(a, b);
  int qa, ra, qb, rb;
  axial(layout_, x(a), y(a), qa, ra);
  axial(layout_, x(b), y(b), qb, rb);
  for (int i = 0; i <= n; ++i) {
    const double t = (double)i / n;
    // Cube coordinates (q, r, -q-r), nudged a hair so a line along a cell edge always rounds the same way.
    double fq = qa + (qb - qa) * t + 1e-6, fr = ra + (rb - ra) * t + 2e-6, fs = -(qa + ra) + (-(qb + rb) + (qa + ra)) * t - 3e-6;
    double rq = std::round(fq), rr = std::round(fr), rs = std::round(fs);
    const double dq = std::fabs(rq - fq), dr = std::fabs(rr - fr), ds = std::fabs(rs - fs);
    if (dq > dr && dq > ds) rq = -rr - rs;
    else if (dr > ds) rr = -rq - rs;
    const int q = (int)rq, r = (int)rr;
    int ox, oy;
    if (layout_ == HexLayout::Pointy) {
      oy = r;
      ox = q + (r - (r & 1)) / 2;
    } else {
      ox = q;
      oy = r + (q - (q & 1)) / 2;
    }
    out.push_back(at(ox, oy));
  }
}

bool Grid::ringCells(Cell middle, int rings, std::vector<Cell>& out) const {
  out.clear();
  if (!valid(middle) || rings < 0 || rings > 1 || (rings > 0 && !isHex())) return false;
  out.push_back(middle);
  for (int i = 0; i < 6 && rings > 0; ++i) {
    Cell nb = neighbor(middle, i);
    if (nb == kNoCell) {
      out.clear();
      return false;
    }
    out.push_back(nb);
  }
  return true;
}

}  // namespace ote
