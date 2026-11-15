#include "core/grid/grid.h"

#include <cstdlib>

namespace ote {

Grid::Grid(GridType type, int width, int height)
    : type_(type), width_(width), height_(height) {}

Cell Grid::at(int x, int y) const {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) return kNoCell;
  return y * width_ + x;
}

int Grid::neighbors(Cell c, Cell out[kMaxNeighbors]) const {
  int n = 0;
  int cx = x(c), cy = y(c);
  switch (type_) {
    case GridType::Square: {
      static const int dx[4] = {0, 1, 0, -1};
      static const int dy[4] = {-1, 0, 1, 0};
      for (int i = 0; i < 4; ++i) {
        Cell nb = at(cx + dx[i], cy + dy[i]);
        if (nb != kNoCell) out[n++] = nb;
      }
      break;
    }
  }
  return n;
}

int Grid::distance(Cell a, Cell b) const {
  switch (type_) {
    case GridType::Square:
      return std::abs(x(a) - x(b)) + std::abs(y(a) - y(b));
  }
  return 0;
}

void Grid::within(Cell c, int radius, std::vector<Cell>& out) const {
  out.clear();
  if (!valid(c) || radius < 0) return;
  switch (type_) {
    case GridType::Square: {
      int cx = x(c), cy = y(c);
      int y0 = cy - radius < 0 ? 0 : cy - radius, y1 = cy + radius >= height_ ? height_ - 1 : cy + radius;
      for (int yy = y0; yy <= y1; ++yy) {
        int span = radius - std::abs(yy - cy);
        int x0 = cx - span < 0 ? 0 : cx - span, x1 = cx + span >= width_ ? width_ - 1 : cx + span;
        for (int xx = x0; xx <= x1; ++xx) out.push_back(yy * width_ + xx);
      }
      break;
    }
  }
}

}  // namespace ote
