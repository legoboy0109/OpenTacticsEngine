#include "ui/dialogue_portrait.h"

namespace ote {

PortraitArt portraitArt(const Database& db, const Sprites* sprites, int styleIdx, const std::string& id) {
  PortraitArt art;
  const GraphicsSet* set = db.graphics.set(styleIdx);
  if (id.empty() || !set || !sprites) return art;
  const Look* look = set->portrait(id);
  if (!look || !sprites->has(*look) || look->cells.empty()) return art;
  const SheetDef& sh = db.graphics.sheets[(size_t)look->sheet];
  art.tex = sprites->texture(look->sheet);
  art.sx = look->cells[0].col * sh.cellW;
  art.sy = look->cells[0].row * sh.cellH;
  art.sw = sh.cellW;
  art.sh = sh.cellH;
  art.faces = look->faces;
  return art;  // (the sheet's "smooth" flag is not honoured: the 2D renderer samples every texture the same way)
}

}  // namespace ote
