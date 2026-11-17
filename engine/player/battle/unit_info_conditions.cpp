// Unit details: the Status page (docs/SRPG.md section 9). Only a unit that has a condition has one. Each condition is a row with its
// name in violet and how long it lasts on the right ("3 turns left", "Until cured"), then the description wrapped under it, as the
// Skills page does. A game without conditions has no such page.
#include <algorithm>

#include "battle/condition_text.h"
#include "battle/unit_info.h"
#include "ui/text.h"

namespace ote {
namespace {

struct Row {
  std::string name, left, description;
};

const Color kName(215, 170, 255), kLabel(150, 158, 190), kSmall(128, 136, 170);

void drawRows(Renderer& r, int x, int y, int w, int h, int lineH, const std::vector<Row>& rows) {
  const int rh = lineH + 2;
  int cy = y;
  for (size_t i = 0; i < rows.size(); ++i) {
    const Row& s = rows[i];
    std::vector<std::string> desc;
    if (!s.description.empty()) desc = wrapText(r, s.description, w - 6);
    int need = rh * (1 + (int)desc.size());
    const int room = y + h - cy - (i + 1 == rows.size() ? 0 : rh);  // keep a line for "+N more" unless this is the last
    while (need > room && desc.size() > 1) { desc.pop_back(); need -= rh; }
    if (need > room) {
      r.drawText(x, cy, ("+" + intStr((int)(rows.size() - i)) + ConditionWords::kMore).c_str(), kLabel);
      return;
    }
    const int lw = textW(r, s.left);
    r.drawText(x, cy, fitText(r, s.name, w - lw - 8).c_str(), kName);
    r.drawText(x + w - lw, cy, s.left.c_str(), kSmall);
    for (size_t k = 0; k < desc.size(); ++k) r.drawText(x + 6, cy + rh * (1 + (int)k), desc[k].c_str(), kLabel);
    cy += need + 3;
  }
}

}  // namespace

void addConditionsPage(UnitInfoPages& pages, const Battle& b, int unit) {
  if (unit < 0 || unit >= (int)b.units.size() || b.db->conditions.empty() || b.units[(size_t)unit].conditions.empty()) return;
  std::vector<Row> rows;
  for (const ConditionState& cs : b.units[(size_t)unit].conditions) {
    const ConditionDef& d = b.db->conditions[(size_t)cs.condition];
    rows.push_back({d.name, conditionDurationText(*b.db, cs), d.description});
  }
  UnitInfoPage p;
  p.title = ConditionWords::kPage;
  p.draw = [rows = std::move(rows)](Renderer& r, int x, int y, int w, int h, int lh) { drawRows(r, x, y, w, h, lh, rows); };
  pages.push_back(p);
}

}  // namespace ote
