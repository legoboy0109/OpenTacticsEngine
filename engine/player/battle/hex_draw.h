// Drawing hexagons with the renderer's rectangles (docs/HEX.md "Top-down picture"). A filled hexagon is a few
// horizontal runs (rows of equal width merged), an outline is the border of those rows: both are exact at any
// scale (the renderer scales whole pixels), neighbours share their edges without a gap or an overlap, and a
// translucent fill never doubles up where hexagons meet. Geometry: core/view/hex_geo.h.
#pragma once
#include <vector>

#include "battle/battle_draw.h"
#include "core/view/hex_geo.h"

namespace ote {

// Fills the hexagon whose box has its top-left at (bx, by) on the screen.
void fillHex(Renderer& r, const HexGeo& g, int bx, int by, Color c);

// The border of the hexagon, `width` pixels thick, inside the hexagon. Only the edges whose bit is set in `edges` are
// drawn (bit i = direction i of the grid, the contract's neighbour order; 63 = all six).
void hexEdges(Renderer& r, const HexGeo& g, int bx, int by, Color c, int width = 1, unsigned edges = 63);

// The edges of `cell` that face a neighbour NOT in `covered` (or the map's rim): what an owner frame around a group
// of hexagons draws.
unsigned outerEdges(const Grid& grid, Cell cell, const std::vector<Cell>& covered);

// Terrain of one hexagon: its colour as a filled hexagon, then the art (a cell of the box size, drawn in the box as it
// is) or, without art, a faint outline and the placeholder letter. (bx, by) = the box on screen.
void drawTerrainHex(Renderer& r, const DrawCtx& c, const HexGeo& g, Cell cell, int bx, int by);

// The same drawing calls for both kinds of grid: a mark over one cell.
struct CellShape {
  bool hex = false;
  HexGeo geo;
  int t = 24;
  static CellShape of(const Grid& grid, int tile) {
    CellShape s;
    s.hex = grid.isHex();
    s.t = tile;
    if (s.hex) s.geo = HexGeo::of(grid, tile);
    return s;
  }
  // Where the cell's box (the tile on squares) is on the screen.
  void origin(const Grid& grid, Cell cell, int camX, int camY, int& sx, int& sy) const;
  void fill(Renderer& r, int sx, int sy, Color c) const;
};

}  // namespace ote
