// Experience and level ups (progression module).
#include "core/battle/battle.h"

namespace ote {

void Battle::awardXp(int unit, int amount, std::vector<Event>& events, bool fromCombat) {
  const RulesConfig& rules = db->rules;
  if (!rules.progression || amount <= 0 || !units[(size_t)unit].alive || units[(size_t)unit].level >= rules.maxLevel) return;
  if (fromCombat && summonGivesNoXp(unit, events)) return;  // the foe was a summon that gives no experience (docs/SRPG.md "Summoning")
  amount = skillXp(units[(size_t)unit], amount);  // "xpPercent" skills (docs/SRPG.md); unchanged without any
  if (amount <= 0) return;

  units[(size_t)unit].xp += amount;
  Event gained;
  gained.type = EventType::XpGained;
  gained.a = unit;
  gained.b = amount;
  events.push_back(gained);

  // The unit is looked up again in every round: srpgLevelUp may promote it, and a promotion's story events may
  // add units (which moves `units`).
  while (units[(size_t)unit].xp >= rules.xpPerLevel && units[(size_t)unit].level < rules.maxLevel) {
    Unit& u = units[(size_t)unit];
    u.xp -= rules.xpPerLevel;
    ++u.level;
    Event up;
    up.type = EventType::LevelUp;
    up.a = unit;
    up.b = u.level;
    up.data.assign(u.stats.size(), 0);
    for (size_t s = 0; s < u.stats.size(); ++s) {
      // A growth of 130 means +1 guaranteed and a 30% chance of +2. growthOf is the one place that says what a
      // unit's growth is (its type's, plus its character's and its skills': docs/SRPG.md "Characters").
      int growth = growthOf(u, (int)s);
      int gain = growth / 100;
      if (growth % 100 > 0 && rng.chance(growth % 100)) ++gain;
      gain = raiseStat(u, (int)s, gain);  // a stat cap may cut it (docs/SRPG.md section 7); without caps unchanged
      up.data[s] = gain;
      if ((int)s == rules.statHp) u.hp += gain;
    }
    events.push_back(up);
    srpgLevelUp(unit, events);  // skills learned at the new level, an automatic promotion; `u` is stale after this
  }
  if (units[(size_t)unit].level >= rules.maxLevel) units[(size_t)unit].xp = 0;
}

}  // namespace ote
