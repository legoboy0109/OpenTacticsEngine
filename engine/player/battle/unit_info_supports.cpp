// Unit details: the Supports page (docs/SRPG.md "Player UI > Unit details", step U5). One block per partner: the
// partner's name, the level, a bar towards the next level and "Conversation ready" when it can be played. A game
// without supports, or a unit that is not a character in any pair, gets no page.
#include <algorithm>

#include "battle/support_text.h"
#include "battle/unit_info.h"
#include "ui/chrome.h"
#include "ui/text.h"

namespace ote {

void addSupportsPage(UnitInfoPages& pages, const Battle& b, int unit) {
  if (b.db->supports.pairs.empty() || unit < 0 || (size_t)unit >= b.units.size()) return;
  std::vector<SupportRow> rows = supportRows(*b.db, b.supports, b.units[(size_t)unit].character);
  if (rows.empty()) return;
  UnitInfoPage page;
  page.title = "Supports";
  page.draw = [rows](Renderer& r, int x, int y, int w, int h, int lineH) {
    const int blockH = 2 * lineH + 8;  // name + level, the bar (with the ready note beside it), a gap
    size_t fit = (size_t)std::max(1, (h + 8) / blockH);
    size_t shown = rows.size() <= fit ? rows.size() : fit - 1;  // leave a row for "+N more" when cut
    if (shown == 0) shown = 1;
    for (size_t i = 0; i < shown; ++i) {
      const SupportRow& row = rows[i];
      const int ry = y + (int)i * blockH;
      std::string level = supportLevelText(row);
      const int levelW = textW(r, level);
      r.drawText(x, ry, fitText(r, row.partner, std::max(20, w - levelW - 10)).c_str(), chrome::kTitle);
      r.drawText(x + w - levelW, ry, level.c_str(), row.level > 0 ? chrome::kText : chrome::kDim);
      // The bar: filled towards the next level; full at the top level.
      const int by = ry + lineH + 2, bh = std::max(4, lineH / 2);
      const int barW = std::max(30, w * 2 / 5);
      r.fillRect(x, by, barW, bh, Color(40, 44, 70));
      r.fillRect(x, by, (int)(barW * supportBarFraction(row)), bh, row.ready ? chrome::kAccent : Color(110, 150, 230));
      r.drawRect(x, by, barW, bh, chrome::kDim);
      std::string note = row.ready ? "Conversation ready" : row.top ? "Highest level" : "";
      if (!note.empty())
        r.drawText(x + barW + 8, ry + lineH, fitText(r, note, std::max(20, w - barW - 8)).c_str(),
                   row.ready ? chrome::kAccent : chrome::kDim);
    }
    if (shown < rows.size())
      r.drawText(x, y + (int)shown * blockH, ("+" + std::to_string(rows.size() - shown) + " more").c_str(), chrome::kDim);
  };
  pages.push_back(std::move(page));
}

}  // namespace ote
