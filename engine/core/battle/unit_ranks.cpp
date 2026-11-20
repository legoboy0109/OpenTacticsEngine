// Unit ranks (docs/SRPG.md "Unit ranks"): a table of named steps on one counter. The rank is never stored: it is read off the counter
// (UnitRankRules::rankAt), so saves, roster carry-over and the stat / skill sources need nothing of their own. The counter is the unit's experience
// in the old mode ("rules.progression.levels", which replaces levelling) or its own `merit` ("rules.ranks", beside levelling and promotion);
// `rankPointsOf` (unit_stats.h) is the one accessor that says which. This file holds the part that changes state: awarding points and announcing a rank-up.
#include <algorithm>

#include "core/battle/battle.h"
#include "core/battle/unit_stats.h"

namespace ote {

int Battle::unitRankPoints(const Unit& u) const { return rankPointsOf(*db, u); }

int Battle::unitRank(const Unit& u) const { return db->rules.unitRanks.rankAt(rankPointsOf(*db, u)); }

// Called by awardXp (progression.cpp) after the skills' and the difficulty's share is worked out. Experience stops at the top rank's `at`.
void Battle::awardRankXp(int unit, int amount, std::vector<Event>& events) {
  const UnitRankRules& table = db->rules.unitRanks;
  Unit& u = units[(size_t)unit];
  amount = std::min(amount, table.topAt() - u.xp);
  if (amount <= 0) return;
  const int before = u.xp;
  u.xp += amount;
  Event gained;
  gained.type = EventType::XpGained;
  gained.a = unit;
  gained.b = amount;
  events.push_back(gained);
  applyRankPoints(unit, before, u.xp, events);
}

// "rules.ranks": merit goes to the units of every side (the table says how much per hit and per defeat; no difficulty or skill share applies) and
// stops at the top rank's `at`. No XpGained-like event: the rank-up is the only news.
void Battle::awardMerit(int unit, int amount, std::vector<Event>& events) {
  if (unit < 0 || unit >= (int)units.size() || amount <= 0 || !units[(size_t)unit].alive) return;
  Unit& u = units[(size_t)unit];
  const int before = u.merit;
  u.merit = std::min(u.merit + amount, db->rules.unitRanks.topAt());
  if (u.merit > before) applyRankPoints(unit, before, u.merit, events);
}

// The consequences of the rank counter (experience or merit, whichever drives the table) having moved from `before` to `after`; nothing when the
// rank is the same. The counter already holds `after`.
void Battle::applyRankPoints(int unit, int before, int after, std::vector<Event>& events) {
  const UnitRankRules& table = db->rules.unitRanks;
  Unit& u = units[(size_t)unit];
  const int rankBefore = table.rankAt(before), rankNow = table.rankAt(after);
  if (rankNow == rankBefore) return;
  // A rank that raises the hit point maximum arrives filled, like a level up's HP gain; one that lowers it cuts the hit points down (never below 1).
  // The old maximum is worked out by putting the old counter back for a moment.
  int& counter = table.merit ? u.merit : u.xp;
  const int maxNow = maxHp(u);
  counter = before;
  const int maxBefore = maxHp(u);
  counter = after;
  if (maxNow > maxBefore) u.hp += maxNow - maxBefore;
  u.hp = std::max(1, std::min(u.hp, maxNow));
  Event up;
  up.type = EventType::UnitRankUp;
  up.a = unit;
  up.b = rankNow;
  up.c = rankBefore;
  up.data.assign(db->stats.size(), 0);
  for (size_t s = 0; s < up.data.size(); ++s) up.data[s] = table.bonusAt(after, (int)s) - table.bonusAt(before, (int)s);
  events.push_back(up);
}

}  // namespace ote
