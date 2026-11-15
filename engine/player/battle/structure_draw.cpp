// Drawing of one structure: picture or placeholder block over its whole
// footprint, the owner's frame around the footprint, and the move/attack
// highlight re-applied on top (the picture would otherwise hide the tint that
// the terrain pass painted). Rules: docs/STRUCTURES.md "Drawing".
// On hexagon maps (docs/HEX.md "Structures"): the picture is drawn in the hexagon's box, or with rings across the
// box of all seven hexagons; the placeholder fills each covered hexagon; the owner frame is the outline around the
// covered hexagons (their outer edges only).
#include <algorithm>

#include "battle/battle_draw.h"
#include "battle/hex_draw.h"
#include "ui/text.h"

namespace ote {
namespace {

// The hexagon version of drawStructure (the square code below stays exactly as it was).
void drawStructureHex(Renderer& r, const DrawCtx& c, int index, int camX, int camY, const Overlay& ov) {
  const Battle& b = c.battle;
  const StructureState st = shownStructure(c, index);
  const StructureDef& def = b.defOf(st);
  const HexGeo geo = HexGeo::of(b.grid, c.layout.tile);
  int bx, by, bw, bh;
  geo.structureBox(st.x, st.y, def.rings, bx, by, bw, bh);
  const int sx = bx - camX, sy = by - camY;
  if (sx + bw < 0 || sx > c.layout.vw || sy + bh < 0 || sy - bh > c.layout.vh) return;

  std::vector<Cell> cells;
  b.structureCells(index, cells);  // the middle hexagon first, then the ring
  auto boxOf = [&](Cell cell, int& x, int& y) {
    geo.box(b.grid.x(cell), b.grid.y(cell), x, y);
    x -= camX;
    y -= camY;
  };

  if (const Look* art = structureArt(c, st)) {
    drawStructureArt(r, c, *art, sx, sy, bw, bh);
  } else {
    // Placeholder: every covered hexagon filled in the structure's colour, a dark outline round the whole, the letter
    // on the middle hexagon.
    Color col = Color::rgb(structureRgb(c, st.type));
    for (Cell cell : cells) {
      int x, y;
      boxOf(cell, x, y);
      fillHex(r, geo, x, y, col);
    }
    for (Cell cell : cells) {
      int x, y;
      boxOf(cell, x, y);
      hexEdges(r, geo, x, y, Color(0, 0, 0, 200), 1, outerEdges(b.grid, cell, cells));
    }
    const StyleDef* style = c.style;
    if (!cells.empty() && def.glyph != ' ' && !(style && style->showGlyphs == 0)) {
      int x, y;
      boxOf(cells[0], x, y);
      drawGlyphCentered(r, x + geo.boxW() / 2, y + geo.boxH() / 2, def.glyph, readableOn(col), c.layout.tile * 9 / 20);
    }
  }

  const int owner = shownOwner(c, index);
  if (owner >= 0) {
    Color oc = Color::rgb(b.sides[(size_t)owner].color);
    for (Cell cell : cells) {
      int x, y;
      boxOf(cell, x, y);
      hexEdges(r, geo, x, y, oc, 2, outerEdges(b.grid, cell, cells));
    }
  }

  if (ov.any()) {
    for (Cell cell : cells) {
      int x, y;
      boxOf(cell, x, y);
      if (ov.reach[(size_t)cell]) fillHex(r, geo, x, y, Color(50, 110, 255, 105));
      else if (ov.attack[(size_t)cell]) fillHex(r, geo, x, y, Color(255, 50, 50, 105));
    }
  }
}

}  // namespace

void drawStructure(Renderer& r, const DrawCtx& c, int index, int camX, int camY, const Overlay& ov) {
  if (c.battle.grid.isHex()) {
    drawStructureHex(r, c, index, camX, camY, ov);
    return;
  }
  const Battle& b = c.battle;
  const StructureState st = shownStructure(c, index);  // fog: kind and owner as the viewer last saw them
  const StructureDef& def = b.defOf(st);
  const int t = c.layout.tile;
  const int fw = def.width * t, fh = def.height * t;
  const int sx = st.x * t - camX, sy = st.y * t - camY;
  // Off screen? (A tall picture may stick out above, at most one footprint's height.)
  if (sx + fw < 0 || sx > c.layout.vw || sy + fh < 0 || sy - fh > c.layout.vh) return;

  if (const Look* art = structureArt(c, st)) {
    drawStructureArt(r, c, *art, sx, sy, fw, fh);
  } else {
    // Placeholder: a block over the whole footprint, inset one pixel, one-pixel dark outline, letter centred.
    Color col = Color::rgb(structureRgb(c, st.type));
    r.fillRect(sx + 1, sy + 1, fw - 2, fh - 2, Color(0, 0, 0, 200));
    r.fillRect(sx + 2, sy + 2, fw - 4, fh - 4, col);
    r.fillRect(sx + 2, sy + 2, fw - 4, 1, mix(col, Color(255, 255, 255), 0.35f));  // soft top highlight
    const StyleDef* style = c.style;
    if (def.glyph != ' ' && !(style && style->showGlyphs == 0))
      drawGlyphCentered(r, sx + fw / 2, sy + fh / 2, def.glyph, readableOn(col), t * 9 / 20 * std::min(def.width, def.height));
  }

  // Owner: a frame in the owner's colour around the whole footprint.
  const int owner = shownOwner(c, index);  // fog: the owner the viewer last saw
  if (owner >= 0) {
    Color oc = Color::rgb(b.sides[(size_t)owner].color);
    r.fillRect(sx, sy, fw, 2, oc);
    r.fillRect(sx, sy + fh - 2, fw, 2, oc);
    r.fillRect(sx, sy + 2, 2, fh - 4, oc);
    r.fillRect(sx + fw - 2, sy + 2, 2, fh - 4, oc);
  }

  if (ov.any()) {
    for (int dy = 0; dy < def.height; ++dy)
      for (int dx = 0; dx < def.width; ++dx) {
        Cell cell = b.grid.at(st.x + dx, st.y + dy);
        if (cell == kNoCell) continue;
        if (ov.reach[(size_t)cell]) r.fillRect(sx + dx * t, sy + dy * t, t, t, Color(50, 110, 255, 105));
        else if (ov.attack[(size_t)cell]) r.fillRect(sx + dx * t, sy + dy * t, t, t, Color(255, 50, 50, 105));
      }
  }
}

}  // namespace ote
