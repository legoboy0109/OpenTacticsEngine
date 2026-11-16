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
    // A block: the partner and the level, the bar, and the note ("Conversation ready") beside the bar when it fits,
    // else on a line of its own (phone-size text).
    const int gap = 8, barW = std::max(30, w * 2 / 5);
    auto noteOf = [](const SupportRow& row) { return row.ready ? "Conversation ready" : row.top ? "Highest level" : ""; };
    auto blockH = [&](const SupportRow& row) {
      const std::string n = noteOf(row);
      const bool below = !n.empty() && barW + 8 + textW(r, n) > w;
      return (2 + (below ? 1 : 0)) * lineH + gap;
    };
    // How many blocks fit; when some are cut the last visible slot is given to "+N more".
    size_t shown = 0;
    int used = 0;
    for (const SupportRow& row : rows) {
      if (used + blockH(row) - gap > h) break;
      used += blockH(row);
      ++shown;
    }
    if (shown < rows.size() && shown > 0 && used + lineH > h) --shown;
    if (shown == 0) shown = 1;
    int ry = y;
    for (size_t i = 0; i < shown; ++i) {
      const SupportRow& row = rows[i];
      std::string level = supportLevelText(row);
      const int levelW = textW(r, level);
      r.drawText(x, ry, fitText(r, row.partner, std::max(20, w - levelW - 10)).c_str(), chrome::kTitle);
      r.drawText(x + w - levelW, ry, level.c_str(), row.level > 0 ? chrome::kText : chrome::kDim);
      // The bar: filled towards the next level; full at the top level.
      const int by = ry + lineH + 2, bh = std::max(4, lineH / 2);
      r.fillRect(x, by, barW, bh, Color(40, 44, 70));
      r.fillRect(x, by, (int)(barW * supportBarFraction(row)), bh, row.ready ? chrome::kAccent : Color(110, 150, 230));
      r.drawRect(x, by, barW, bh, chrome::kDim);
      const std::string note = noteOf(row);
      const Color nc = row.ready ? chrome::kAccent : chrome::kDim;
      if (!note.empty()) {
        if (barW + 8 + textW(r, note) <= w) r.drawText(x + barW + 8, ry + lineH, note.c_str(), nc);
        else r.drawText(x, ry + 2 * lineH, fitText(r, note, w).c_str(), nc);
      }
      ry += blockH(row);
    }
    if (shown < rows.size()) r.drawText(x, ry, ("+" + std::to_string(rows.size() - shown) + " more").c_str(), chrome::kDim);
  };
  pages.push_back(std::move(page));
}

}  // namespace ote
