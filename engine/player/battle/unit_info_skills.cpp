// Unit details: the Skills page (step U1). A game without skills has no such page.
#include "battle/unit_info.h"
#include "battle/unit_info_rows.h"

namespace ote {

void addSkillsPage(UnitInfoPages& pages, const Battle& b, int unit) {
  if (unit < 0 || unit >= (int)b.units.size() || b.db->skills.empty()) return;
  std::vector<SkillRow> rows = skillRows(b, b.units[(size_t)unit]);
  bool any = false;  // a game whose only "skills" are the hidden ones of conditions has no Skills page
  for (const SkillDef& s : b.db->skills) any = any || !s.hidden;
  if (!any) return;
  UnitInfoPage p;
  p.title = "Skills";
  p.draw = [rows = std::move(rows)](Renderer& r, int x, int y, int w, int h, int lh) {
    drawSkillRows(r, x, y, w, h, lh, rows);
  };
  pages.push_back(p);
}

}  // namespace ote
