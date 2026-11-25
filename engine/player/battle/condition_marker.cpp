#include "battle/condition_marker.h"

#include <algorithm>

#include "battle/battle_view.h"
#include "battle/condition_text.h"
#include "ui/text.h"

namespace ote {

std::string conditionMarkerText(const DrawCtx& c, const Unit& u) {
  if (u.id < 0 || (size_t)u.id >= c.view.units.size()) return "";
  if (c.view.units[(size_t)u.id].downed) return DownedWords::kTag;  // docs/SRPG.md "Downed units and revive"
  if (c.battle.db->conditions.empty()) return "";
  const std::vector<int>& conds = c.view.units[(size_t)u.id].conds;
  if (conds.empty()) return "";
  return conditionTag(*c.battle.db, conds[0]) + (conds.size() > 1 ? "+" : "");
}

void drawConditionMarker(Renderer& r, const DrawCtx& c, const Unit& u, int left, int bottom, float alpha) {
  const std::string text = conditionMarkerText(c, u);
  if (text.empty()) return;
  // A downed unit is drawn see-through; its marker reads as clearly as an upright unit's (docs/SRPG.md "Downed units and revive").
  if (u.id >= 0 && (size_t)u.id < c.view.units.size() && c.view.units[(size_t)u.id].downed) alpha = std::min(1.0f, alpha / kDownedAlpha);
  auto a = [&](Color col) { return withAlpha(col, (int)(col.a * alpha)); };
  const int w = textW(r, text) + 2, h = c.layout.lh + 1;
  r.fillRect(left - 1, bottom - h - 1, w + 2, h + 2, a(Color(0, 0, 0, 215)));  // dark rim, so it reads on any colour
  r.fillRect(left, bottom - h, w, h, a(Color(70, 40, 110, 235)));
  r.drawText(left + 1, bottom - h + 1, text.c_str(), a(Color(235, 205, 255)));
}

}  // namespace ote
