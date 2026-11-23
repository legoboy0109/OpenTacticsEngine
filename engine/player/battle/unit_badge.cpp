#include "battle/unit_badge.h"

#include <algorithm>
#include <cmath>

namespace ote {

bool badgeApplies(const DrawCtx& c, const BadgeDef& badge, const Unit& u) {
  const Battle& b = c.battle;
  const Database& db = *b.db;
  bool tagged = false;
  for (const std::string& t : db.unitTypes[(size_t)u.type].tags) tagged = tagged || t == badge.tag;
  if (!tagged && u.character >= 0 && (size_t)u.character < db.characters.size())
    for (const std::string& t : db.characters[(size_t)u.character].tags) tagged = tagged || t == badge.tag;
  if (!tagged) return false;
  const SideState& side = b.sides[(size_t)shownSide(c, u)];
  switch (badge.sides) {
    case BadgeDef::Sides::All: return true;
    case BadgeDef::Sides::Computer: return side.ai;
    case BadgeDef::Sides::Player: return !side.ai;
    case BadgeDef::Sides::List: return std::find(badge.sideIds.begin(), badge.sideIds.end(), side.id) != badge.sideIds.end();
  }
  return false;
}

void drawUnitBadges(Renderer& r, const DrawCtx& c, const Unit& u, int left, int top, int w, int h, float alpha) {
  const Graphics& g = c.battle.db->graphics;
  if (g.badges.empty() || !c.sprites) return;
  const float k = (float)w / (float)std::max(1, c.battle.db->view.tileSize);  // one pixel of the game's tile size on screen
  const int a = std::max(0, std::min(255, (int)(alpha * 255.0f + 0.5f)));
  for (const BadgeDef& badge : g.badges) {
    if (!badgeApplies(c, badge, u)) continue;
    const TextureId tex = c.sprites->texture(badge.sheet);
    if (!tex) continue;
    const SheetDef& sh = g.sheets[(size_t)badge.sheet];
    const int sw = badge.whole ? sh.width : sh.cellW, shh = badge.whole ? sh.height : sh.cellH;
    const int sx = badge.whole ? 0 : badge.cell.col * sh.cellW, sy = badge.whole ? 0 : badge.cell.row * sh.cellH;
    const int dw = std::max(1, (int)std::lround(sw * k)), dh = std::max(1, (int)std::lround(shh * k));
    int x = left, y = top;  // the badge's top left corner, by the anchor
    switch (badge.anchor) {
      case BadgeDef::Anchor::TopLeft: break;
      case BadgeDef::Anchor::TopRight: x = left + w - dw; break;
      case BadgeDef::Anchor::BottomLeft: y = top + h - dh; break;
      case BadgeDef::Anchor::BottomRight: x = left + w - dw; y = top + h - dh; break;
      case BadgeDef::Anchor::Center: x = left + (w - dw) / 2; y = top + (h - dh) / 2; break;
    }
    x += (int)std::lround(badge.dx * k);
    y += (int)std::lround(badge.dy * k);
    r.drawTexture(tex, sx, sy, sw, shh, x, y, dw, dh, Color(255, 255, 255, (uint8_t)a));
  }
}

void drawMapMarker(Renderer& r, const DrawCtx& c, int marker, int left, int top, int w, int h) {
  const Graphics& g = c.battle.db->graphics;
  if (marker < 0 || (size_t)marker >= g.markers.size() || !c.sprites) return;
  const MarkerDef& m = g.markers[(size_t)marker];
  const TextureId tex = c.sprites->texture(m.sheet);
  if (!tex) return;
  const float k = (float)w / (float)std::max(1, c.battle.db->view.tileSize);
  const SheetDef& sh = g.sheets[(size_t)m.sheet];
  const int sw = m.whole ? sh.width : sh.cellW, shh = m.whole ? sh.height : sh.cellH;
  const int sx = m.whole ? 0 : m.cell.col * sh.cellW, sy = m.whole ? 0 : m.cell.row * sh.cellH;
  const int dw = std::max(1, (int)std::lround(sw * k)), dh = std::max(1, (int)std::lround(shh * k));
  int x = left, y = top;
  switch (m.anchor) {
    case BadgeDef::Anchor::TopLeft: break;
    case BadgeDef::Anchor::TopRight: x = left + w - dw; break;
    case BadgeDef::Anchor::BottomLeft: y = top + h - dh; break;
    case BadgeDef::Anchor::BottomRight: x = left + w - dw; y = top + h - dh; break;
    case BadgeDef::Anchor::Center: x = left + (w - dw) / 2; y = top + (h - dh) / 2; break;
  }
  r.drawTexture(tex, sx, sy, sw, shh, x + (int)std::lround(m.dx * k), y + (int)std::lround(m.dy * k), dw, dh);
}

}  // namespace ote
