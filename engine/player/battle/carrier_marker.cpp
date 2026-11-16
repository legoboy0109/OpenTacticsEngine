#include "battle/carrier_marker.h"

#include <algorithm>

#include "ui/text.h"

namespace ote {

bool carrierMarked(const DrawCtx& c, const Unit& u) {
  if (u.carrying < 0 || (size_t)u.carrying >= c.battle.units.size()) return false;
  const size_t other = (size_t)u.carrying;
  return other < c.view.units.size() ? !c.view.units[other].visible : true;
}

void drawCarrierMarker(Renderer& r, const DrawCtx& c, const Unit& u, int right, int top, int unitW, float alpha) {
  if (!carrierMarked(c, u)) return;
  const Unit& other = c.battle.units[(size_t)u.carrying];
  Color tab = Color::rgb(c.battle.sides[(size_t)other.side].color);  // the carried unit's side (the same as the carrier's)
  const int w = std::max(4, unitW / 3), h = std::max(5, unitW * 5 / 12);
  auto a = [&](Color col) { return withAlpha(col, (int)(col.a * alpha)); };
  r.fillRect(right - w - 1, top - 1, w + 2, h + 2, a(Color(0, 0, 0, 220)));  // dark rim, so it reads on any colour
  r.fillRect(right - w, top, w, h, a(tab));
  r.fillRect(right - w, top, w, 1, a(mix(tab, Color(255, 255, 255), 0.45f)));  // soft top highlight, like the unit block
}

}  // namespace ote
