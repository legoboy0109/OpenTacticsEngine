// The unit pictures in the preparation screen's lists (Units, and the lists of whose items / trade partner / shop buyer): each row
// shows the unit's map picture (its standing animation's frame of the moment, in the player's side colour as on the map), its name,
// and under the name its level and class. A unit type with no picture in any style gets the map's own placeholder: the side-coloured
// block with the type's letter.
#include <algorithm>

#include "battle/battle_draw.h"
#include "battle/battle_scene.h"
#include "ui/text.h"

namespace ote {

void BattleScene::drawUnitIcon(Renderer& r, int type, int x, int y, int size) {
  const Database& db = session_.db;
  if (type < 0 || (size_t)type >= db.unitTypes.size()) return;
  int side = 0;  // the roster belongs to the player's side: the first one a person plays
  for (size_t i = 0; i < battle_.sides.size(); ++i)
    if (!battle_.sides[i].ai) { side = (int)i; break; }
  const Color col = Color::rgb(battle_.sides[(size_t)side].color);
  DrawCtx ctx{battle_, view_, layout_, cam_, time_, &db.styles[styleIdx_], host_.sprites(), styleIdx_, &proj_, host_.isoTiles(), map3d_.get()};
  for (int k = -1; k < (int)db.styles.size(); ++k) {  // the style being played first (what the map shows), then any other that has a picture
    const int style = k < 0 ? styleIdx_ : k;
    if (k >= 0 && k == styleIdx_) continue;
    const GraphicsSet* set = db.graphics.set(style);
    if (!set || (size_t)type >= set->units.size()) continue;
    const Look& look = set->units[(size_t)type].forSide(battle_.sides[(size_t)side].id);
    if (!ctx.sprites || !ctx.sprites->has(look)) continue;
    const SpriteSrc s = lookSrc(ctx, look);  // the frame of the moment: an animated look idles
    // As on the map (drawUnitArt): one square wide, the height keeps the cell's proportions, feet on the bottom edge. A frame
    // taller than it is wide (a rider, a big weapon) therefore stands out over the top of its box instead of being shrunk to
    // fit, which made such units half the size of the others in one list.
    const int dw = size, dh = std::max(1, dw * s.h / std::max(1, s.w));
    if (db.graphics.unitMarker) {  // the flat base in the side's colour, as on the map
      const int bh = std::max(2, size / 6);
      r.fillRect(x, y + size - bh, size, bh, Color(20, 20, 28, 200));
      r.fillRect(x + 1, y + size - bh + 1, size - 2, bh - 2, col);
    }
    r.drawTexture(s.tex, s.x, s.y, s.w, s.h, x + (size - dw) / 2, y + size - dh, dw, dh, Color(255, 255, 255));
    return;
  }
  r.fillRect(x, y, size, size, Color(0, 0, 0, 200));
  r.fillRect(x + 1, y + 1, size - 2, size - 2, col);
  r.fillRect(x + 1, y + 1, size - 2, 1, mix(col, Color(255, 255, 255), 0.35f));
  drawGlyphCentered(r, x + size / 2, y + size / 2, db.unitTypes[(size_t)type].glyph, readableOn(col), std::max(1, (size - 2) * 3 / 4));
}

void BattleScene::prepLive(const std::string& id, int& type, int& level) const {
  if (id.empty()) return;
  for (const Unit& u : battle_.units)
    if (u.storyId == id && battle_.exists(u)) {
      type = u.type;
      level = u.level;
      return;
    }
}

void BattleScene::decorateUnitRow(PanelRow& row, int type, int level) {
  const Database& db = session_.db;
  if (type < 0 || (size_t)type >= db.unitTypes.size()) return;
  row.iconSize = std::max(layout_.lh + 2, std::min(layout_.tile, 2 * layout_.lh));
  row.icon = [this, type](Renderer& r, int x, int y, int size) { drawUnitIcon(r, type, x, y, size); };
  row.sub = "Lv" + intStr(level) + " " + db.unitTypes[(size_t)type].name;
}

}  // namespace ote
