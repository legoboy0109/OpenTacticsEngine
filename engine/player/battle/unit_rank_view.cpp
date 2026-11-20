#include "battle/unit_rank_view.h"

#include "battle/battle_view.h"
#include "ui/text.h"

namespace ote {

int shownRank(const DrawCtx& c, const Unit& u) {
  const Battle& b = c.battle;
  if (!b.db->rules.unitRanks.on()) return -1;
  if (u.id >= 0 && (size_t)u.id < c.view.units.size() && c.view.units[(size_t)u.id].rank != kRankOwn) return c.view.units[(size_t)u.id].rank;
  return b.unitRank(u);
}

std::string rankBadgeText(const DrawCtx& c, const Unit& u) {
  const int rank = shownRank(c, u);
  return rank < 0 ? std::string() : c.battle.db->rules.unitRanks.levels[(size_t)rank].badge;
}

void drawRankBadge(Renderer& r, const DrawCtx& c, const Unit& u, int right, int bottom, float alpha) {
  const std::string text = rankBadgeText(c, u);
  if (text.empty()) return;
  auto a = [&](Color col) { return withAlpha(col, (int)(col.a * alpha)); };
  const int w = textW(r, text) + 2, h = c.layout.lh + 1;
  r.fillRect(right - w - 1, bottom - h - 1, w + 2, h + 2, a(Color(0, 0, 0, 215)));  // dark rim, so it reads on any colour
  r.fillRect(right - w, bottom - h, w, h, a(Color(120, 86, 20, 240)));
  r.drawText(right - w + 1, bottom - h + 1, text.c_str(), a(Color(255, 232, 130)));
}

}  // namespace ote
