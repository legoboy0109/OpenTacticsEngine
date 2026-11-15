#include "battle/hex_draw.h"

#include <algorithm>

#include "ui/text.h"

namespace ote {

void fillHex(Renderer& r, const HexGeo& g, int bx, int by, Color c) {
  const int rows = g.boxH();
  int runStart = 0, runL = 0, runR = -1;  // rows [runStart, row) all span [runL, runR)
  for (int row = 0; row <= rows; ++row) {
    int l = 0, rr = -1;
    if (row < rows) g.rowSpan(row, l, rr);
    if (row > 0 && (row == rows || l != runL || rr != runR)) {
      if (runR > runL) r.fillRect(bx + runL, by + runStart, runR - runL, row - runStart, c);
      runStart = row;
    }
    runL = l;
    runR = rr;
  }
}

namespace {

// The direction a run on the left / right side of row `row` belongs to (bit numbers of the contract's order).
int leftDir(const HexGeo& g, int row) {
  if (g.pointy) return row < g.quarter() ? 5 : row < 3 * g.quarter() ? 4 : 3;
  return row < g.T / 2 ? 5 : 4;
}
int rightDir(const HexGeo& g, int row) {
  if (g.pointy) return row < g.quarter() ? 0 : row < 3 * g.quarter() ? 1 : 2;
  return row < g.T / 2 ? 1 : 2;
}

}  // namespace

void hexEdges(Renderer& r, const HexGeo& g, int bx, int by, Color c, int width, unsigned edges) {
  const int rows = g.boxH(), w = std::max(1, width);
  for (int row = 0; row < rows; ++row) {
    int l, rr;
    g.rowSpan(row, l, rr);
    // The inside of the border: narrower than the row by `w` on each side, and inside the rows `w` above and below
    // as well (that thickens the border along the slanted edges and fills the rows near a corner or flat edge).
    int il = l + w, ir = rr - w;
    bool inside = row - w >= 0 && row + w < rows;
    if (inside)
      for (int dy = -w; dy <= w; dy += 2 * w) {
        int l2, r2;
        g.rowSpan(row + dy, l2, r2);
        il = std::max(il, l2);
        ir = std::min(ir, r2);
      }
    auto run = [&](int from, int to, int dir) {
      if (to > from && ((edges >> dir) & 1u)) r.fillRect(bx + from, by + row, to - from, 1, c);
    };
    if (inside && il < ir) {
      run(l, il, leftDir(g, row));
      run(ir, rr, rightDir(g, row));
    } else if (!g.pointy && !inside) {  // the flat top or bottom edge: the whole row
      run(l, rr, row < rows / 2 ? 0 : 3);
    } else {  // a corner of a pointy hexagon: the row is all border, split at the middle
      int mid = g.boxW() / 2;
      run(l, mid, leftDir(g, row));
      run(mid, rr, rightDir(g, row));
    }
  }
}

unsigned outerEdges(const Grid& grid, Cell cell, const std::vector<Cell>& covered) {
  unsigned mask = 0;
  for (int d = 0; d < grid.directionCount(); ++d) {
    Cell nb = grid.neighbor(cell, d);
    if (nb == kNoCell || std::find(covered.begin(), covered.end(), nb) == covered.end()) mask |= 1u << d;
  }
  return mask;
}

void drawTerrainHex(Renderer& r, const DrawCtx& c, const HexGeo& g, Cell cell, int bx, int by) {
  const Battle& b = c.battle;
  const CellState& cs = b.cells[cell];
  const TerrainDef& td = b.terrainAt(cell);
  const StyleDef* st = c.style;
  Color col = Color::rgb(terrainRgb(c, cs.terrain));
  fillHex(r, g, bx, by, col);
  if (const Look* art = terrainArt(c, cs.terrain)) {
    // The art's cell has the box's size; its transparent corners show what was drawn before (the neighbour or
    // the background). The engine does not cut it to the hexagon.
    drawTerrainArt(r, c, *art, cell, bx, by, g.boxW(), g.boxH());
  } else {
    hexEdges(r, g, bx, by, Color(0, 0, 0, 28));
    if (td.glyph != ' ' && !(st && st->showGlyphs == 0)) {
      int t = c.layout.tile;
      drawGlyphCentered(r, bx + g.boxW() / 2, by + g.boxH() / 2, td.glyph, mix(col, readableOn(col), 0.28f), t * 9 / 20);
    }
  }
}

void CellShape::origin(const Grid& grid, Cell cell, int camX, int camY, int& sx, int& sy) const {
  if (hex) {
    geo.box(grid.x(cell), grid.y(cell), sx, sy);
  } else {
    sx = grid.x(cell) * t;
    sy = grid.y(cell) * t;
  }
  sx -= camX;
  sy -= camY;
}

void CellShape::fill(Renderer& r, int sx, int sy, Color c) const {
  if (hex) fillHex(r, geo, sx, sy, c);
  else r.fillRect(sx, sy, t, t, c);
}

}  // namespace ote
