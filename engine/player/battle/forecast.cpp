// Combat forecast box shown while choosing an attack target.
#include <algorithm>

#include "battle/battle_draw.h"
#include "battle/item_text.h"
#include "battle/mapweapon_text.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {
namespace {

struct Line {
  std::string text;
  Color color;
};
using Column = std::vector<Line>;

const Color kText(235, 237, 245), kGrey(130, 136, 165), kNote(255, 220, 120);
constexpr size_t kMaxNotes = 3;  // note lines per column; the rest are summed up as "+N more"

// The lines of one fighter. `first`: it strikes before its foe in each exchange (the defender, see Forecast::
// defenderFirst). `notes`: the activations of this fighter that could fire (Forecast::notes), in roll order.
Column makeColumn(Renderer& r, int maxW, const Battle& b, const Unit& u, const Unit& foe, const StrikeForecast& sf, int hpAfter, bool showHit,
                  bool showCrit, bool counterMissing, bool first, const std::vector<ForecastNote>& notes) {
  Column col;
  Color sc = Color::rgb(b.sides[u.side].color);
  col.push_back({u.name, mix(sc, Color(255, 255, 255), 0.35f)});
  if (counterMissing) {
    col.push_back({"No counter", kGrey});
  } else {
    std::string name = b.db->weapons[sf.weapon].name;
    const int mapStructure = b.mapWeaponUsedBy(u, u.cell, sf.weapon);  // fired from the structure it stands on (docs/SRPG.md "Map weapons")
    if (mapStructure >= 0) name = mapWeaponForecastName(b, mapStructure);
    else if (b.db->rules.items.carriedWeapons) {  // the weapon item's uses left, "Iron Sword 12"
      int slot = b.weaponSlot(u, sf.weapon);
      if (slot >= 0 && u.items[(size_t)slot].uses >= 0) name += " " + intStr(u.items[(size_t)slot].uses);
    }
    col.push_back({name, kGrey});
    if (mapStructure >= 0) col.push_back({mapWeaponForecastFrom(b, mapStructure), kNote});
    // Damage types (docs/SRPG.md "Damage types"), only in a game that declares them: "Fire", or "Fire  res +3" with the foe's resistance.
    const int type = b.db->weapons[sf.weapon].damageType;
    if (type >= 0) {
      std::string line = b.db->damageTypes.list[(size_t)type].name;
      const int rs = b.resistOf(foe, type);
      if (rs == 0) {
        col.push_back({line, kGrey});
      } else {
        const std::string res = std::string("res ") + (rs > 0 ? "+" : "") + intStr(rs);
        // One line ("Fire  res +3") when the column is wide enough (a big UI scale narrows it), else the resistance on a line of its own:
        // the number must never be the part that is cut off.
        if (textW(r, line + "  " + res) <= maxW) col.push_back({line + "  " + res, kGrey});
        else { col.push_back({line, kGrey}); col.push_back({res, kGrey}); }
      }
    }
  }
  col.push_back({"HP " + intStr(u.hp) + " > " + intStr(hpAfter), hpAfter <= 0 ? Color(255, 110, 100) : kText});
  if (!counterMissing) {
    std::string dmg = "Dmg " + intStr(sf.damage);
    // "Dmg 7 x2 x2": the strikes of the exchange, then (skills with multiStrike) the strikes in a row of each.
    if (sf.strikes > 1 || sf.multi > 1) dmg += " x" + intStr(sf.strikes);
    if (sf.multi > 1) dmg += " x" + intStr(sf.multi);
    if (!sf.each.empty()) {  // strikes that differ (a skill asks which strike it is): each one's damage, "Dmg 9/4"
      dmg = "Dmg ";
      for (size_t i = 0; i < sf.each.size(); ++i) dmg += (i ? "/" : "") + intStr(sf.each[i].damage);
    }
    col.push_back({dmg, kText});
    if (showHit) col.push_back({"Hit " + intStr(sf.hit) + "%", kText});
    if (showCrit) col.push_back({"Crit " + intStr(sf.crit) + "%", kText});
    if (first) col.push_back({"Strikes first", kNote});
  }
  size_t shown = 0, mine = 0;
  for (const ForecastNote& n : notes) {
    if (n.unit != u.id || n.skill < 0 || n.skill >= (int)b.db->skills.size()) continue;
    ++mine;
    if (shown >= kMaxNotes) continue;
    col.push_back({b.db->skills[(size_t)n.skill].name + " " + intStr(n.chance) + "%", kNote});
    ++shown;
  }
  if (mine > shown) col.push_back({"+" + intStr((int)(mine - shown)) + " more", kGrey});
  return col;
}

}  // namespace

void drawForecast(Renderer& r, const DrawCtx& c, int attacker, int defender, int weapon, Cell targetCell, const std::string& extra) {
  const Battle& b = c.battle;
  const Layout& L = c.layout;
  Forecast f;
  if (!b.forecast(attacker, defender, weapon, b.units[attacker].cell, f)) return;
  const Unit& a = b.units[attacker];
  const Unit& d = b.units[defender];

  int hpD = std::max(0, d.hp - f.attack.allDamage());
  int hpA = a.hp;
  if (f.counter.can) hpA = std::max(0, a.hp - f.counter.allDamage());
  // Hide hit/crit rows for games without hit/crit formulas (everything 100% / 0%).
  bool showHit = f.attack.hit < 100 || (f.counter.can && f.counter.hit < 100);
  bool showCrit = f.attack.crit > 0 || (f.counter.can && f.counter.crit > 0);

  Column ca = makeColumn(r, (L.W - 8 - 24) / 2, b, a, d, f.attack, hpA, showHit, showCrit, false, false, f.notes);
  Column cd = makeColumn(r, (L.W - 8 - 24) / 2, b, d, a, f.counter, hpD, showHit, showCrit, !f.counter.can, f.defenderFirst, f.notes);
  if (b.apGame()) ca.push_back({"AP " + intStr(b.apAttackCost(attacker, weapon)), kNote});  // action points: what this attack costs
  if (!f.counter.can) std::swap(cd[1], cd[2]);  // keep HP right under the name
  size_t rows = std::max(ca.size(), cd.size());
  // The lines, once per change, for the player tests (drawn every frame, so not logged every frame).
  static std::string lastLog;
  std::string text;
  for (const Column* col : {&ca, &cd}) {
    for (const Line& ln : *col) text += ln.text + "; ";
    text += "| ";
  }
  if (!extra.empty()) text += extra + "; ";
  if (text != lastLog) {
    lastLog = text;
    OTE_INFO("forecast: %s", text.c_str());
  }

  // Column width follows the widest line (capped so the box fits the map view);
  // longer lines are truncated with "..".
  int colW = 0;
  for (const Column* col : {&ca, &cd})
    for (const Line& ln : *col) colW = std::max(colW, textW(r, ln.text));
  // Up to the whole screen width (not only the map's): a narrow window would cut "Heavy Blow 40%" otherwise. The box
  // is centred on the map and pushed over the panel only as far as it needs.
  colW = std::min(colW, (L.W - 8 - 24) / 2);
  int rh = L.row, w = colW * 2 + 24, h = (int)rows * rh + 12 + (extra.empty() ? 0 : rh);
  int x = std::max(4, std::min((L.vw - w) / 2, L.W - 4 - w));
  int ty = 0;
  if (targetCell >= 0) {
    int tx;
    float fx, fy;
    cellCentre(b.grid, L.tile, targetCell, fx, fy);
    c.toScreen(fx, fy, tx, ty);
  }
  int y = ty < L.vh / 2 ? L.vh - h - 6 : 6;  // opposite half from the target
  r.fillRect(x + 2, y + 2, w, h, Color(0, 0, 0, 110));
  r.fillRect(x, y, w, h, Color(22, 26, 46, 245));
  r.drawRect(x, y, w, h, Color(255, 220, 90));
  r.fillRect(x + w / 2, y + 4, 1, h - 8 - (extra.empty() ? 0 : rh), Color(70, 76, 110));
  if (!extra.empty()) r.drawText(x + 8, y + 6 + (int)rows * rh, fitText(r, extra, w - 16).c_str(), kNote);
  for (size_t i = 0; i < ca.size(); ++i)
    r.drawText(x + 8, y + 6 + (int)i * rh, fitText(r, ca[i].text, colW).c_str(), ca[i].color);
  for (size_t i = 0; i < cd.size(); ++i)
    r.drawText(x + w / 2 + 8, y + 6 + (int)i * rh, fitText(r, cd[i].text, colW).c_str(), cd[i].color);
}

}  // namespace ote
