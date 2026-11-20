// The forecast of an area attack (docs/SRPG.md "Area attacks and line fire"): the box the player sees while aiming such a weapon.
// With a unit on the aimed square this is the normal forecast box of that exchange plus one line for everybody else the attack hits;
// aimed at an empty square it is a small box of its own. Display only.
#include <algorithm>

#include "battle/battle_draw.h"
#include "battle/item_text.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {
namespace {

constexpr size_t kMaxListed = 5;  // numbers listed in a line; more are cut off with ".."

// "5, 7, ally 4": what one strike does to each seen victim (not `skip`), enemies first, units of the attacker's own side after.
std::string damageList(const AreaForecast& f, int skip, int& count) {
  std::string out;
  count = 0;
  size_t listed = 0;
  for (int ally = 0; ally < 2; ++ally) {
    for (size_t i = 0; i < f.victims.size(); ++i) {
      const AreaVictim& v = f.victims[i];
      if ((int)i == skip || !v.seen || v.enemy == (ally == 1)) continue;
      ++count;
      if (++listed > kMaxListed) continue;
      out += (out.empty() ? "" : ", ") + std::string(ally ? "ally " : "") + intStr(v.hit.damage);
    }
  }
  if (count > (int)kMaxListed) out += "..";
  return out;
}

}  // namespace

std::string areaOthersText(const Battle&, const AreaForecast& f) {
  int count = 0;
  const std::string list = damageList(f, f.primary, count);
  if (count == 0) return std::string();
  return "+" + intStr(count) + " more: " + list + " dmg";
}

void drawAreaForecast(Renderer& r, const DrawCtx& c, int attacker, int weapon, Cell cell) {
  const Battle& b = c.battle;
  AreaForecast f;
  if (!b.areaForecast(attacker, weapon, b.units[(size_t)attacker].cell, cell, f)) return;
  // A foe on the aimed square: its exchange in the usual box, the rest of the victims on one more line.
  if (f.primary >= 0 && f.victims[(size_t)f.primary].enemy && f.victims[(size_t)f.primary].seen) {
    drawForecast(r, c, attacker, f.victims[(size_t)f.primary].unit, weapon, cell, areaOthersText(b, f));
    return;
  }
  const Layout& L = c.layout;
  const Unit& a = b.units[(size_t)attacker];
  const Color kText(235, 237, 245), kGrey(130, 136, 165), kNote(255, 220, 120), kWarn(255, 140, 120);
  struct Line { std::string text; Color color; };
  std::vector<Line> lines;
  lines.push_back({a.name, mix(Color::rgb(b.sides[a.side].color), Color(255, 255, 255), 0.35f)});
  lines.push_back({b.db->weapons[(size_t)weapon].name, kGrey});
  int foes = 0, friends = 0;
  for (const AreaVictim& v : f.victims) {
    if (!v.seen) continue;
    (v.enemy ? foes : friends) += 1;
  }
  int count = 0;
  const std::string list = damageList(f, -1, count);
  lines.push_back({"Hits " + intStr(foes) + (foes == 1 ? " foe" : " foes"), kText});
  if (!list.empty()) lines.push_back({list + " dmg", kText});
  if (b.apGame()) lines.push_back({"AP " + intStr(b.apAttackCost(attacker, weapon)), kNote});  // action points: what the attack costs
  if (friends > 0) lines.push_back({intStr(friends) + (friends == 1 ? " ally in the way" : " allies in the way"), kWarn});
  if (f.primary >= 0 && !f.victims[(size_t)f.primary].enemy) lines.push_back({"Aimed at an ally", kNote});

  std::string text;
  for (const Line& ln : lines) text += ln.text + "; ";
  static std::string lastLog;
  if (text != lastLog) {
    lastLog = text;
    OTE_INFO("area forecast: %s", text.c_str());
  }

  int colW = 0;
  for (const Line& ln : lines) colW = std::max(colW, textW(r, ln.text));
  colW = std::min(colW, L.W - 8 - 16);
  const int rh = L.row, w = colW + 16, h = (int)lines.size() * rh + 12;
  const int x = std::max(4, std::min((L.vw - w) / 2, L.W - 4 - w));
  int ty = 0;
  int tx;
  float fx, fy;
  cellCentre(b.grid, L.tile, cell, fx, fy);
  c.toScreen(fx, fy, tx, ty);
  const int y = ty < L.vh / 2 ? L.vh - h - 6 : 6;  // opposite half from the target
  r.fillRect(x + 2, y + 2, w, h, Color(0, 0, 0, 110));
  r.fillRect(x, y, w, h, Color(22, 26, 46, 245));
  r.drawRect(x, y, w, h, Color(255, 220, 90));
  for (size_t i = 0; i < lines.size(); ++i) r.drawText(x + 8, y + 6 + (int)i * rh, fitText(r, lines[i].text, colW).c_str(), lines[i].color);
}

}  // namespace ote
