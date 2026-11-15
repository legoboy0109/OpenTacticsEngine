// Grid topology: the ONLY place that knows how cells connect.
//
// Rules code (movement, ranges, AI) must go through neighbors()/distance()
// and never do x/y arithmetic, so new layouts (hex, isometric = square with
// a different renderer, stacked 3D) only need changes here and in the
// player's drawing code. Hexagons: docs/HEX.md ("Coordinates" is the contract).
#pragma once
#include <vector>

#include "core/base/types.h"

namespace ote {

enum class GridType { Square, Hex };

// Which way the hexagons point (game.json "grid": { "type": "hex", "layout": "pointy" | "flat" }).
// Pointy: rows run straight across, odd rows sit half a hexagon right. Flat: columns run straight down,
// odd columns sit half a hexagon down.
enum class HexLayout { Pointy, Flat };

class Grid {
 public:
  static constexpr int kMaxNeighbors = 8;
  static constexpr int kMaxDirections = 6;

  Grid() = default;
  Grid(GridType type, int width, int height, HexLayout layout = HexLayout::Pointy);

  GridType type() const { return type_; }
  HexLayout layout() const { return layout_; }  // meaningful for hexagon grids only
  bool isHex() const { return type_ == GridType::Hex; }
  int width() const { return width_; }
  int height() const { return height_; }
  int cellCount() const { return width_ * height_; }

  // Storage coordinates (column, row). For rendering and map files only.
  Cell at(int x, int y) const;  // kNoCell when out of bounds
  int x(Cell c) const { return c % width_; }
  int y(Cell c) const { return c / width_; }
  bool valid(Cell c) const { return c >= 0 && c < cellCount(); }

  // Writes adjacent cells that are on the map into out[]; returns how many. Order is fixed:
  // square N, E, S, W; hexagons as docs/HEX.md lists them (a subsequence of neighbor(c, 0..) in direction order).
  int neighbors(Cell c, Cell out[kMaxNeighbors]) const;
  // How many directions a cell has: 4 (square) or 6 (hexagon). Directions are what neighbors() enumerates;
  // join-piece bit i, art and the editor refer to direction i.
  int directionCount() const { return isHex() ? 6 : 4; }
  // The neighbour of `c` in direction `dir` (0..directionCount()-1), or kNoCell when it is off the map
  // (or `dir`/`c` is out of range).
  Cell neighbor(Cell c, int dir) const;
  // The direction in which `to` is a neighbour of `from`, or -1 when it is not adjacent.
  int directionTo(Cell from, Cell to) const;
  // Steps between two cells ignoring terrain (used for attack ranges).
  int distance(Cell a, Cell b) const;
  // Every cell within `radius` steps of `c` (distance() <= radius), `c` included; ascending cell index.
  void within(Cell c, int radius, std::vector<Cell>& out) const;
  // The cells of a hexagon structure: `middle`, then its six neighbours in direction order when rings == 1
  // (rings == 0: just `middle`). False (and `out` cleared) when any of them is off the map, `middle` is
  // invalid, rings is not 0 or 1, or the grid is not a hexagon grid and rings > 0.
  bool ringCells(Cell middle, int rings, std::vector<Cell>& out) const;

 private:
  GridType type_ = GridType::Square;
  HexLayout layout_ = HexLayout::Pointy;
  int width_ = 0;
  int height_ = 0;
};

}  // namespace ote
