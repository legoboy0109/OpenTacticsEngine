// Where the hexagons of a hexagon map are in the top-down picture (docs/HEX.md "Top-down picture").
// Pure arithmetic, no drawing: the player's projection, its tests and (later) the editor share it.
//
// "Flat map pixels" are the pixels BattleView animates units in: on a square map cell (x, y) spans
// x*tile .. (x+1)*tile; on a hexagon map a cell's position is its hexagon's MIDDLE (a unit's tile-sized box is
// centred on it), so everything that says "the cell's tile box" goes through cellTopLeft() below.
#pragma once
#include "core/data/database.h"
#include "core/grid/grid.h"

namespace ote {

struct HexGeo {
  bool pointy = true;
  int T = 24;  // view.tileSize: the short side of a hexagon's box
  int L = 28;  // the long side: the multiple of 4 nearest to T * 1.1547
  int cols = 0, rows = 0;

  // L for a tile size (24 -> 28, 16 -> 20, 32 -> 36).
  static int longSide(int tile);
  // Geometry of a hexagon grid at a tile size. (For a square grid it is not meaningful.)
  static HexGeo of(const Grid& g, int tile);
  // The same lattice with the box sizes given: `shortSide` (T) and `longSide` (L). Pointy: T wide, L tall (L a
  // multiple of 4, T even); flat: L wide, T tall. The 2D isometric view lays its squashed hexagons out with it.
  static HexGeo ofBox(const Grid& g, int shortSide, int longSide);

  // Box of one hexagon: pointy T wide and L tall, flat L wide and T tall. A quarter of the long side is the
  // step that rows (pointy) or columns (flat) overlap by: rows/columns are 3 * quarter() apart.
  int boxW() const { return pointy ? T : L; }
  int boxH() const { return pointy ? L : T; }
  int quarter() const { return L / 4; }
  // Top-left of the box of cell (x, y) and its middle point, in flat map pixels. (x, y) may be off the map.
  void box(int x, int y, int& bx, int& by) const;
  void middle(int x, int y, int& mx, int& my) const;
  // Size of the whole map picture.
  int mapW() const;
  int mapH() const;
  // The six corners of cell (x, y)'s hexagon in flat map pixels, clockwise as seen from above (y down), starting at the
  // top corner (pointy) or the top-left one (flat). Edge i runs from corner i to corner i + 1 and faces direction i of
  // the contract's neighbour order (both layouts), so the corners of neighbours coincide exactly.
  void corners(int x, int y, float (&out)[6][2]) const;
  // The box around a structure whose middle hexagon is (x, y): one hexagon's box, or with rings the box across
  // all seven (pointy 3T x 5L/2, flat 5L/2 x 3T). The bottom edge of the picture sits on its bottom.
  void structureBox(int x, int y, int rings, int& bx, int& by, int& bw, int& bh) const;

  // The lattice position whose middle is nearest to the point (may be off the map: x < 0 and so on).
  void nearestXY(float fx, float fy, int& x, int& y) const;
  // The cell whose middle is nearest to the point, kNoCell when that position is not on the map.
  Cell pick(float fx, float fy) const;
  // The same but never off the map: the nearest cell of the map (for points that may lie a little outside).
  Cell nearestCell(float fx, float fy) const;

  // The filled hexagon inside a box, row by row (0 .. boxH()-1): pixels [l, r) of the row, relative to the
  // box's left edge. Neighbouring hexagons share their edges exactly (no gap, no overlap); the hexagon touches
  // the box on all four sides. Odd tile sizes work but may leave a one-pixel seam: use even ones.
  void rowSpan(int row, int& l, int& r) const;
};

// ---- Positions that work on both kinds of grid (squares: unchanged arithmetic).
// Middle of a cell in flat map pixels ((x + 0.5) * tile on squares).
void cellCentre(const Grid& g, int tile, Cell c, float& fx, float& fy);
// Top-left of the cell's tile-sized box: x * tile on squares; on hexagon maps the box of size tile x tile
// centred on the hexagon's middle (where a unit's picture stands).
void cellTopLeft(const Grid& g, int tile, Cell c, float& fx, float& fy);
// Box of a structure's picture/footprint in flat map pixels (squares: width x height squares from its corner).
void structureBox(const Grid& g, int tile, int x, int y, const StructureDef& def, int& bx, int& by, int& bw, int& bh);
// The cell a unit drawn with its tile box at (px, py) is on (nearest), clamped to the map.
Cell cellNearPos(const Grid& g, int tile, float px, float py);

}  // namespace ote
