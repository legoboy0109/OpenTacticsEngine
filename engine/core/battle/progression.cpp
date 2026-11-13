// Experience and level ups (progression module).
#include "core/battle/battle.h"

namespace ote {

void Battle::awardXp(int unit, int amount, std::vector<Event>& events) {
  const RulesConfig& rules = db->rules;
  Unit& u = units[unit];
  if (!rules.progression || amount <= 0 || !u.alive || u.level >= rules.maxLevel) return;

  u.xp += amount;
  Event gained;
  gained.type = EventType::XpGained;
  gained.a = unit;
  gained.b = amount;
  events.push_back(gained);

  const UnitTypeDef& type = typeOf(u);
  while (u.xp >= rules.xpPerLevel && u.level < rules.maxLevel) {
    u.xp -= rules.xpPerLevel;
    ++u.level;
    Event up;
    up.type = EventType::LevelUp;
    up.a = unit;
    up.b = u.level;
    up.data.assign(u.stats.size(), 0);
    for (size_t s = 0; s < u.stats.size(); ++s) {
      // A growth of 130 means +1 guaranteed and a 30% chance of +2.
      int growth = s < type.growths.size() ? type.growths[s] : 0;
      int gain = growth / 100;
      if (growth % 100 > 0 && rng.chance(growth % 100)) ++gain;
      u.stats[s] += gain;
      up.data[s] = gain;
      if ((int)s == rules.statHp) u.hp += gain;
    }
    events.push_back(up);
  }
  if (u.level >= rules.maxLevel) u.xp = 0;
}

}  // namespace ote
