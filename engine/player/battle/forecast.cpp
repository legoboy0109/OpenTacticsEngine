// Combat forecast box shown while choosing an attack target.
#include <algorithm>

#include "battle/battle_draw.h"
#include "ui/text.h"

namespace ote {
namespace {

struct Line {
  std::string text;
  Color color;
};
using Column = std::vector<Line>;

const Color kText(235, 237, 245), kGrey(130, 136, 165);

Column makeColumn(const Battle& b, const Unit& u, const StrikeForecast& sf, int hpAfter, bool showHit,
                  bool showCrit, bool counterMissing) {
  Column col;
  Color sc = Color::rgb(b.sides[u.side].color);
  col.push_back({u.name, mix(sc, Color(255, 255, 255), 0.35f)});
  if (counterMissing) {
    col.push_back({"No counter", kGrey});
  } else {
    col.push_back({b.db->weapons[sf.weapon].name, kGrey});
  }
  col.push_back({"HP " + intStr(u.hp) + " > " + intStr(hpAfter), hpAfter <= 0 ? Color(255, 110, 100) : kText});
  if (!counterMissing) {
    std::string dmg = "Dmg " + intStr(sf.damage);
    if (sf.strikes > 1) dmg += " x" + intStr(sf.strikes);
    col.push_back({dmg, kText});
    if (showHit) col.push_back({"Hit " + intStr(sf.hit) + "%", kText});
    if (showCrit) col.push_back({"Crit " + intStr(sf.crit) + "%", kText});
  }
  return col;
}

}  // namespace

void drawForecast(Renderer& r, const DrawCtx& c, int attacker, int defender, int weapon, Cell targetCell) {
  const Battle& b = c.battle;
  const Layout& L = c.layout;
  Forecast f;
  if (!b.forecast(attacker, defender, weapon, b.units[attacker].cell, f)) return;
  const Unit& a = b.units[attacker];
  const Unit& d = b.units[defender];

  int hpD = std::max(0, d.hp - f.attack.damage * f.attack.strikes);
  int hpA = a.hp;
  if (f.counter.can) hpA = std::max(0, a.hp - f.counter.damage * f.counter.strikes);
  // Hide hit/crit rows for games without hit/crit formulas (everything 100% / 0%).
  bool showHit = f.attack.hit < 100 || (f.counter.can && f.counter.hit < 100);
  bool showCrit = f.attack.crit > 0 || (f.counter.can && f.counter.crit > 0);

  Column ca = makeColumn(b, a, f.attack, hpA, showHit, showCrit, false);
  Column cd = makeColumn(b, d, f.counter, hpD, showHit, showCrit, !f.counter.can);
  if (!f.counter.can) std::swap(cd[1], cd[2]);  // keep HP right under the name
  size_t rows = std::max(ca.size(), cd.size());

  // Column width follows the widest line (capped so the box fits the map view);
  // longer lines are truncated with "..".
  int colW = 0;
  for (const Column* col : {&ca, &cd})
    for (const Line& ln : *col) colW = std::max(colW, textW(r, ln.text));
  colW = std::min(colW, (L.vw - 24) / 2);
  int rh = L.row, w = colW * 2 + 24, h = (int)rows * rh + 12;
  int x = (L.vw - w) / 2;
  int ty = targetCell >= 0 ? b.grid.y(targetCell) * L.tile - c.cam.iy() : 0;
  int y = ty < L.vh / 2 ? L.vh - h - 6 : 6;  // opposite half from the target
  r.fillRect(x + 2, y + 2, w, h, Color(0, 0, 0, 110));
  r.fillRect(x, y, w, h, Color(22, 26, 46, 245));
  r.drawRect(x, y, w, h, Color(255, 220, 90));
  r.fillRect(x + w / 2, y + 4, 1, h - 8, Color(70, 76, 110));
  for (size_t i = 0; i < ca.size(); ++i)
    r.drawText(x + 8, y + 6 + (int)i * rh, fitText(r, ca[i].text, colW).c_str(), ca[i].color);
  for (size_t i = 0; i < cd.size(); ++i)
    r.drawText(x + w / 2 + 8, y + 6 + (int)i * rh, fitText(r, cd[i].text, colW).c_str(), cd[i].color);
}

}  // namespace ote
