// Unit resources in a battle (pools_api.h / pools_internal.h; docs/SRPG.md "Unit resources (MP and the like)"): a pool per unit, sized by a stat, paid by
// abilities' "spend" and regenerated at the start of the unit's own turns. Nothing happens in a game without "unitResources".
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

int Battle::poolMax(const Unit& u, int pool) const {
  if (pool < 0 || pool >= (int)db->unitResources.size()) return 0;
  return std::max(0, stat(u, db->unitResources[(size_t)pool].stat));
}

int Battle::poolOf(const Unit& u, int pool) const {
  const int top = poolMax(u, pool);
  if (pool < 0 || pool >= (int)db->unitResources.size()) return 0;
  // No record yet (a unit made before the battle started, an old save): the start share. A record above a maximum that fell (a bonus gone) is cut.
  if ((size_t)pool >= u.pools.size()) return top * db->unitResources[(size_t)pool].start / 100;
  return std::max(0, std::min(top, u.pools[(size_t)pool]));
}

void Battle::notePools(Unit& u) const {
  const size_t n = db->unitResources.size();
  if (u.pools.size() >= n) return;
  std::vector<int> now(n);
  for (size_t p = 0; p < n; ++p) now[p] = poolOf(u, (int)p);
  u.pools = now;
}

void Battle::poolsBattleStart() {
  if (db->unitResources.empty()) return;
  for (Unit& u : units)
    if (u.alive) notePools(u);
}

bool Battle::abilityPriceOk(const Unit& u, int ability, std::string* why) const {
  if (ability < 0 || ability >= (int)db->abilities.size()) return true;
  for (const AbilitySpend& s : db->abilities[(size_t)ability].spend) {
    if (s.unit) {
      if (poolOf(u, s.index) >= s.amount) continue;
      if (why) *why = "needs " + std::to_string(s.amount) + " " + db->unitResources[(size_t)s.index].name;
      return false;
    }
    const SideState& side = sides[(size_t)u.side];
    const int have = (size_t)s.index < side.resources.size() ? side.resources[(size_t)s.index] : 0;
    if (have >= s.amount) continue;
    if (why) *why = "not enough " + db->resources[(size_t)s.index].name;
    return false;
  }
  return true;
}

void Battle::payAbilityPrice(int unit, int ability, std::vector<Event>& events) {
  if (ability < 0 || ability >= (int)db->abilities.size()) return;
  for (const AbilitySpend& s : db->abilities[(size_t)ability].spend) {
    Unit& u = units[(size_t)unit];
    if (!s.unit) {
      changeResource(u.side, s.index, -s.amount, events);
      continue;
    }
    notePools(u);
    const int now = std::max(0, poolOf(u, s.index) - s.amount);
    u.pools[(size_t)s.index] = now;
    Event e;
    e.type = EventType::PoolChanged;
    e.a = unit;
    e.b = s.index;
    e.c = -s.amount;
    e.d = now;
    events.push_back(e);
  }
}

void Battle::poolsTurnStart(std::vector<Event>& events) {
  if (db->unitResources.empty()) return;
  for (size_t i = 0; i < units.size(); ++i) {
    Unit& u = units[i];
    if (!ownsTurn(u) || !exists(u)) continue;
    notePools(u);
    for (size_t p = 0; p < db->unitResources.size(); ++p) {
      const UnitResourceDef& d = db->unitResources[p];
      const int top = poolMax(u, (int)p), had = poolOf(u, (int)p);
      const int now = std::min(top, had + d.regenPerTurn + top * d.regenPercent / 100);
      if (now <= had) continue;
      u.pools[p] = now;
      Event e;
      e.type = EventType::PoolChanged;
      e.a = (int)i;
      e.b = (int)p;
      e.c = now - had;
      e.d = now;
      events.push_back(e);
    }
  }
}

}  // namespace ote
