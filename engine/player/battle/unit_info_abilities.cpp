// Unit details: the Abilities page (docs/SRPG.md "Player UI > Unit details", step U3). One block per ability the
// unit has: its name, what it does, who it is used on, its limits and what is left of them now. No page for a unit
// without abilities (so a game without abilities shows the same tabs as before). Blocks that do not fit the box are
// cut off with "+N more..." (the frame has no scrolling; the action menu lists all of them anyway).
// The page keeps copies of the words, never the Battle (the page lives while the overlay is open).
#include <algorithm>

#include "battle/ability_text.h"
#include "battle/unit_info.h"
#include "ui/text.h"

namespace ote {
namespace {

struct Block {
  std::string name, effect, target, limits, status, description;
};

int drawLines(Renderer& r, const std::string& text, int x, int y, int w, int lineH, Color c, int bottom) {
  if (text.empty()) return y;
  for (const std::string& line : wrapText(r, text, w)) {
    if (y + lineH > bottom) break;
    r.drawText(x, y, line.c_str(), c);
    y += lineH;
  }
  return y;
}

}  // namespace

void addAbilitiesPage(UnitInfoPages& pages, const Battle& b, int unit) {
  if (unit < 0 || (size_t)unit >= b.units.size()) return;
  const Unit& u = b.units[(size_t)unit];
  std::vector<int> ids;
  b.abilitiesOf(u, ids);
  if (ids.empty()) return;
  std::vector<Block> blocks;
  for (int id : ids) {
    const AbilityDef& d = b.db->abilities[(size_t)id];
    Block k;
    k.name = d.name;
    k.effect = abilityEffectText(*b.db, d);
    k.target = "Use on " + abilityTargetText(d);
    if (!abilityRangeText(d).empty()) k.target += ", " + abilityRangeText(d);
    k.target += "; " + abilityCostText(d);
    k.limits = abilityLimitText(d);
    k.status = abilityStatusText(b, u, id);
    k.description = d.description;
    blocks.push_back(k);
  }
  UnitInfoPage page;
  page.title = "Abilities";
  page.draw = [blocks](Renderer& r, int x, int y, int w, int h, int lineH) {
    const int bottom = y + h;
    const Color head(255, 235, 130), text(225, 228, 240), dim(150, 156, 185), note(150, 225, 255);
    for (size_t i = 0; i < blocks.size(); ++i) {
      const Block& k = blocks[i];
      // A block is drawn whole or not at all: its height first (the name row, then each wrapped part).
      auto rows = [&](const std::string& s) { return s.empty() ? 0 : (int)wrapText(r, s, w).size(); };
      int need = 1 + rows(k.effect) + rows(k.target) + rows(k.limits) + rows(k.status) + rows(k.description);
      int left = (int)(blocks.size() - i);
      // Keep a row for "+N more..." unless this is the last block.
      int reserve = left > 1 ? lineH : 0;
      if (y + need * lineH + reserve > bottom && i > 0) {
        std::string more = "+" + intStr(left) + AbilityWords::kMore;
        r.drawText(x, std::min(y, bottom - lineH), more.c_str(), dim);
        return;
      }
      r.drawText(x, y, fitText(r, k.name, w).c_str(), head);
      y += lineH;
      y = drawLines(r, k.effect, x, y, w, lineH, text, bottom);
      y = drawLines(r, k.target, x, y, w, lineH, dim, bottom);
      y = drawLines(r, k.limits, x, y, w, lineH, dim, bottom);
      y = drawLines(r, k.status, x, y, w, lineH, note, bottom);
      y = drawLines(r, k.description, x, y, w, lineH, dim, bottom);
      y += 3;
    }
  };
  pages.push_back(std::move(page));
}

}  // namespace ote
