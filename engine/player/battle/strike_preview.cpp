// What a strike ability would do, for the player's pick (strike_preview.h). The words are placeholders like the rest of the battle menus.
#include "battle/strike_preview.h"

namespace ote {

namespace {

std::string numbers(const StrikeForecast& f) {
  std::string s = std::to_string(f.damage) + " dmg";
  const int times = f.strikes * std::max(1, f.multi);
  if (times > 1) s += " x" + std::to_string(times);
  s += ", " + std::to_string(f.hit) + "% hit";
  if (f.crit > 0) s += ", " + std::to_string(f.crit) + "% crit";
  return s;
}

}  // namespace

std::string strikeUnitLine(const Battle& b, int user, int ability, int target) {
  Forecast f;
  if (!b.abilityStrikeForecast(user, ability, target, b.units[(size_t)user].cell, f) || !f.attack.can) return "";
  return b.db->abilities[(size_t)ability].name + ": " + numbers(f.attack);
}

std::string strikeAreaLine(const Battle& b, int user, int ability, Cell cell, std::vector<Cell>& covered) {
  covered.clear();
  AreaForecast f;
  if (!b.abilityStrikeArea(user, ability, b.units[(size_t)user].cell, cell, f)) return "";
  covered = f.cells;
  std::string list;
  int shown = 0, count = 0;
  for (const AreaVictim& v : f.victims) {
    if (!v.seen) continue;  // fog: an unseen victim is never listed
    ++count;
    if (shown >= 2) continue;  // the hint line is one line: the first two, then the count says the rest
    list += (list.empty() ? "" : ", ") + b.units[(size_t)v.unit].name + (v.enemy ? " " : " (ally) ") + numbers(v.hit);
    ++shown;
  }
  if (count > shown) list += ", ...";
  return b.db->abilities[(size_t)ability].name + ": " + std::to_string(count) + " hit (" + list + ")";
}

}  // namespace ote
