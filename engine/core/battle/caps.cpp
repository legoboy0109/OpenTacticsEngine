// Stat caps (docs/SRPG.md section 7): the highest a permanent stat can grow to. The one place that rule lives; every
// code path that raises Unit::stats (level-up, promotion, a stat-raising item) goes through raiseStat.
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

int Battle::statCap(const Unit& u, int stat) const {
  if (stat < 0) return -1;
  const size_t s = (size_t)stat;
  int cap = s < db->statCaps.size() ? db->statCaps[s] : -1;
  const std::vector<int>& own = typeOf(u).caps;
  if (s < own.size() && own[s] >= 0) cap = own[s];  // the class's cap overrides the game-wide one
  if (cap < 0) return -1;                           // no cap anywhere: a personal adjustment has nothing to adjust
  if (u.character >= 0) {
    const std::vector<int>& adj = db->characters[(size_t)u.character].capAdjust;
    if (s < adj.size()) cap = std::max(0, cap + adj[s]);
  }
  return cap;
}

// A gain is cut to the room left under the cap; a stat already at or over its cap is neither raised nor lowered; a
// negative change always applies. Returns what really changed (the number the events report).
int Battle::raiseStat(Unit& u, int stat, int amount) const {
  if (stat < 0 || (size_t)stat >= u.stats.size()) return 0;
  int& v = u.stats[(size_t)stat];
  if (amount > 0) {
    const int cap = statCap(u, stat);
    if (cap >= 0) amount = std::min(amount, std::max(0, cap - v));
  }
  const int before = v;
  v = std::max(0, v + amount);
  return v - before;
}

// Would these per-stat changes alter anything (a raise item that only names stats at their cap would not)?
bool Battle::canRaiseAny(const Unit& u, const std::vector<int>& gains) const {
  bool named = false;  // a list that names no stat at all is "not capped" (as before caps existed)
  for (size_t s = 0; s < gains.size() && s < u.stats.size(); ++s) {
    if (gains[s] == 0) continue;
    named = true;
    const int cap = statCap(u, (int)s);
    if (gains[s] < 0 || cap < 0 || u.stats[s] < cap) return true;
  }
  return !named;
}

}  // namespace ote
