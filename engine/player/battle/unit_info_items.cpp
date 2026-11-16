// Unit details: the Items page (step U1). Only for a game that has items (or a unit that carries some), else the page
// would always say "No items".
#include "battle/unit_info.h"
#include "battle/unit_info_rows.h"

namespace ote {

void addItemsPage(UnitInfoPages& pages, const Battle& b, int unit) {
  if (unit < 0 || unit >= (int)b.units.size()) return;
  const Unit& u = b.units[(size_t)unit];
  if (u.items.empty() && b.db->items.empty()) return;
  UnitInfoPage p;
  p.title = "Items";
  p.draw = [rows = itemRows(b, u)](Renderer& r, int x, int y, int w, int h, int lh) {
    drawItemRows(r, x, y, w, h, lh, rows);
  };
  pages.push_back(p);
}

}  // namespace ote
