// The forecast box of a staff (docs/SRPG.md section 8 "Player"), the look of the attack forecast (forecast.cpp): the staff's
// name and uses left, one line per unit it would reach ("Tarn  HP 5 > 13"), the hit chance only when the staff can miss, the
// user's experience, and "Breaks" on its last use. At most four unit lines, then "+N more".
#include <algorithm>

#include "battle/battle_draw.h"
#include "battle/staff_text.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {
namespace {

struct Line {
  std::string text;
  Color color;
};

const Color kText(235, 237, 245), kGrey(130, 136, 165), kNote(255, 220, 120), kGood(150, 240, 160);
constexpr size_t kMaxUnits = 4;

}  // namespace

void drawStaffForecast(Renderer& r, const DrawCtx& c, const StaffForecast& f, Cell targetCell) {
  const Battle& b = c.battle;
  const Layout& L = c.layout;
  if (f.item < 0 || (size_t)f.item >= b.db->items.size()) return;
  std::vector<Line> lines;
  std::string head = b.db->items[(size_t)f.item].name;
  if (f.usesLeft >= 0) head += " " + intStr(f.usesLeft);
  lines.push_back({head, kGrey});
  for (size_t i = 0; i < f.hits.size() && i < kMaxUnits; ++i) {
    const StaffHit& h = f.hits[i];
    const Unit& u = b.units[(size_t)h.unit];
    lines.push_back({u.name + "  HP " + intStr(h.hpBefore) + " > " + intStr(h.hpAfter), kGood});
  }
  if (f.hits.size() > kMaxUnits) lines.push_back({"+" + intStr((int)(f.hits.size() - kMaxUnits)) + StaffWords::kMore, kGrey});
  const std::string hit = staffHitText(f);
  if (!hit.empty()) lines.push_back({hit, kText});
  if (f.xp > 0) lines.push_back({"Exp " + intStr(f.xp), kText});
  if (f.breaks) lines.push_back({StaffWords::kBreaks, kNote});

  // The lines, once per change, for the player tests (drawn every frame, so not logged every frame).
  static std::string lastLog;
  std::string text;
  for (const Line& ln : lines) text += ln.text + "; ";
  if (text != lastLog) {
    lastLog = text;
    OTE_INFO("staff forecast: %s", text.c_str());
  }

  // As wide as the widest line, never wider than the screen (longer lines are cut with "..").
  int w = 0;
  for (const Line& ln : lines) w = std::max(w, textW(r, ln.text));
  w = std::min(w, L.W - 8 - 16) + 16;
  const int rh = L.row, h = (int)lines.size() * rh + 12;
  const int x = std::max(4, std::min((L.vw - w) / 2, L.W - 4 - w));
  int ty = 0;
  if (targetCell >= 0) {
    int tx;
    float fx, fy;
    cellCentre(b.grid, L.tile, targetCell, fx, fy);
    c.toScreen(fx, fy, tx, ty);
  }
  const int y = ty < L.vh / 2 ? L.vh - h - 6 : 6;  // the half of the screen away from the target
  r.fillRect(x + 2, y + 2, w, h, Color(0, 0, 0, 110));
  r.fillRect(x, y, w, h, Color(22, 26, 46, 245));
  r.drawRect(x, y, w, h, Color(120, 230, 150));
  for (size_t i = 0; i < lines.size(); ++i) r.drawText(x + 8, y + 6 + (int)i * rh, fitText(r, lines[i].text, w - 16).c_str(), lines[i].color);
}

}  // namespace ote
