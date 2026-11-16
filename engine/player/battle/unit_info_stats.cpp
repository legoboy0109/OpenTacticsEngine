// Unit details: the Stats page (step U1). Growths are not shown (docs/SRPG.md "Unit details").
#include "battle/unit_info.h"
#include "battle/unit_info_rows.h"

namespace ote {

void addStatsPage(UnitInfoPages& pages, const Battle& b, int unit) {
  if (unit < 0 || unit >= (int)b.units.size()) return;
  UnitInfoPage p;
  p.title = "Stats";
  p.draw = [rows = statRows(b, b.units[(size_t)unit])](Renderer& r, int x, int y, int w, int h, int lh) {
    drawStatRows(r, x, y, w, h, lh, rows);
  };
  pages.push_back(p);
}

}  // namespace ote
