// The forecast box of an attack on a door (docs/SRPG.md section 10 "Player"), the look of the staff forecast: the door's name and
// the weapon, the damage and strikes, its hit points before > after and "Breaks" when the door would open.
#include <algorithm>

#include "battle/battle_draw.h"
#include "battle/place_text.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {

void drawPlaceForecast(Renderer& r, const DrawCtx& c, const PlaceForecast& f, Cell targetCell) {
  const Battle& b = c.battle;
  const Layout& L = c.layout;
  if (f.structure < 0 || (size_t)f.structure >= b.structures.size()) return;
  const Color text(235, 237, 245), grey(130, 136, 165), note(255, 220, 120), good(150, 240, 160);
  std::vector<std::pair<std::string, Color>> lines;
  lines.push_back({b.defOf(b.structures[(size_t)f.structure]).name + "  " + b.db->weapons[(size_t)f.weapon].name, grey});
  lines.push_back({std::string(PlaceWords::kDamage) + " " + intStr(f.damage) + (f.strikes > 1 ? " x" + intStr(f.strikes) : ""), text});
  lines.push_back({std::string(PlaceWords::kHp) + " " + intStr(f.hpBefore) + " > " + intStr(f.hpAfter), good});
  if (f.breaks) lines.push_back({PlaceWords::kBreaks, note});
  static std::string lastLog;
  std::string log;
  for (const auto& l : lines) log += l.first + "; ";
  if (log != lastLog) {
    lastLog = log;
    OTE_INFO("door forecast: %s", log.c_str());
  }
  int w = 0;
  for (const auto& l : lines) w = std::max(w, textW(r, l.first));
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
  const int y = ty < L.vh / 2 ? L.vh - h - 6 : 6;
  r.fillRect(x + 2, y + 2, w, h, Color(0, 0, 0, 110));
  r.fillRect(x, y, w, h, Color(22, 26, 46, 245));
  r.drawRect(x, y, w, h, Color(120, 230, 150));
  for (size_t i = 0; i < lines.size(); ++i) r.drawText(x + 8, y + 6 + (int)i * rh, fitText(r, lines[i].first, w - 16).c_str(), lines[i].second);
}

}  // namespace ote
