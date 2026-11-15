// Grid topology: the ONLY place that knows how cells connect.
//
// Rules code (movement, ranges, AI) must go through neighbors()/distance()
// and never do x/y arithmetic, so new layouts (hex, isometric = square with
// a different renderer, stacked 3D) only need changes here and in the
// player's drawing code.
#pragma once
#include <vector>

#include "core/base/types.h"

namespace ote {

enum class GridType { Square /*, Hex (planned) */ };

class Grid {
 public:
  static constexpr int kMaxNeighbors = 8;

  Grid() = default;
  Grid(GridType type, int width, int height);

  GridType type() const { return type_; }
  int width() const { return width_; }
  int height() const { return height_; }
  int cellCount() const { return width_ * height_; }

  // Storage coordinates (column, row). For rendering and map files only.
  Cell at(int x, int y) const;  // kNoCell when out of bounds
  int x(Cell c) const { return c % width_; }
  int y(Cell c) const { return c / width_; }
  bool valid(Cell c) const { return c >= 0 && c < cellCount(); }

  // Writes adjacent cells into out[]; returns how many.
  int neighbors(Cell c, Cell out[kMaxNeighbors]) const;
  // Steps between two cells ignoring terrain (used for attack ranges).
  int distance(Cell a, Cell b) const;
  // Every cell within `radius` steps of `c` (distance() <= radius), `c` included; ascending cell index.
  void within(Cell c, int radius, std::vector<Cell>& out) const;

 private:
  GridType type_ = GridType::Square;
  int width_ = 0;
  int height_ = 0;
};

}  // namespace ote
