// Unit ranks (docs/SRPG.md "Unit ranks"): experience as a running total with named steps instead of levels. The rank is never stored: it is
// read off the unit's saved experience (UnitRankRules::rankAt), so saves, roster carry-over and the stat / skill sources need nothing of
// their own. This file holds the part that changes state: awarding experience and announcing a rank-up.
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

int Battle::unitRank(const Unit& u) const { return db->rules.unitRanks.rankAt(u.xp); }

// Called by awardXp (progression.cpp) after the skills' and the difficulty's share is worked out. Experience stops at the top rank's `at`.
void Battle::awardRankXp(int unit, int amount, std::vector<Event>& events) {
  const UnitRankRules& table = db->rules.unitRanks;
  Unit& u = units[(size_t)unit];
  amount = std::min(amount, table.topAt() - u.xp);
  if (amount <= 0) return;
  const int before = table.rankAt(u.xp), maxBefore = maxHp(u);
  u.xp += amount;
  Event gained;
  gained.type = EventType::XpGained;
  gained.a = unit;
  gained.b = amount;
  events.push_back(gained);

  const int now = table.rankAt(u.xp);
  if (now == before) return;
  // A rank that raises the hit point maximum arrives filled, like a level up's HP gain; one that lowers it cuts the hit points down (never below 1).
  const int maxNow = maxHp(u);
  if (maxNow > maxBefore) u.hp += maxNow - maxBefore;
  u.hp = std::max(1, std::min(u.hp, maxNow));
  Event up;
  up.type = EventType::UnitRankUp;
  up.a = unit;
  up.b = now;
  up.c = before;
  up.data.assign(db->stats.size(), 0);
  for (size_t s = 0; s < up.data.size(); ++s) up.data[s] = table.bonusAt(u.xp, (int)s) - table.bonusAt(u.xp - amount, (int)s);
  events.push_back(up);
}

}  // namespace ote
