// The pick hint of an area ability (ability_area_text.h).
#include "battle/ability_area_text.h"

namespace ote {

namespace {

// What it does to one unit: "+12 HP", "cured", "Sleep 60%", "Protect" (a condition that does not roll is named without a chance).
std::string hitText(const Battle& b, int user, int ability, const AbilityAreaHit& h) {
  const AbilityDef& d = b.db->abilities[(size_t)ability];
  const Unit& t = b.units[(size_t)h.unit];
  std::string s;
  auto add = [&](const std::string& part) { s += (s.empty() ? "" : " ") + part; };
  if (h.revive > 0) add("revived +" + std::to_string(h.revive) + " HP");  // docs/SRPG.md "Downed units and revive"
  if (h.heal > 0) add("+" + std::to_string(h.heal) + " HP");
  if (h.cures > 0) add("cured");
  for (size_t k = 0; k < d.inflicts.size(); ++k) {
    const AbilityInflict& in = d.inflicts[k];
    if (b.inflictWhy(t, in.cond, in.turns) != 0) continue;  // immune, or has it already: nothing to say
    const std::string& name = b.db->conditions[(size_t)in.cond].name;
    add(in.rolls() ? name + " " + std::to_string(b.abilityInflictChance(user, ability, h.unit, b.units[(size_t)user].cell, (int)k)) + "%" : name);
  }
  return s;
}

}  // namespace

std::string abilityAreaLine(const Battle& b, int user, int ability, Cell cell, std::vector<Cell>& covered) {
  covered.clear();
  AbilityAreaForecast f;
  if (!b.abilityAreaForecast(user, ability, b.units[(size_t)user].cell, cell, f)) return "";
  covered = f.cells;
  std::string list;
  int shown = 0, count = 0;
  for (const AbilityAreaHit& h : f.hits) {
    if (!h.seen) continue;  // fog: an unseen unit is never listed
    ++count;
    if (shown >= 2) continue;  // the hint line is one line: the first two, then the count says the rest
    list += (list.empty() ? "" : ", ") + b.units[(size_t)h.unit].name + " " + hitText(b, user, ability, h);
    ++shown;
  }
  if (count > shown) list += ", ...";
  return b.db->abilities[(size_t)ability].name + ": " + std::to_string(count) + (count == 1 ? " unit (" : " units (") + list + ")";
}

}  // namespace ote
