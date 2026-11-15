// Drawing of one structure: picture or placeholder block over its whole
// footprint, the owner's frame around the footprint, and the move/attack
// highlight re-applied on top (the picture would otherwise hide the tint that
// the terrain pass painted). Rules: docs/STRUCTURES.md "Drawing".
#include <algorithm>

#include "battle/battle_draw.h"
#include "ui/text.h"

namespace ote {

void drawStructure(Renderer& r, const DrawCtx& c, int index, int camX, int camY, const Overlay& ov) {
  const Battle& b = c.battle;
  const StructureState& st = b.structures[(size_t)index];
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
