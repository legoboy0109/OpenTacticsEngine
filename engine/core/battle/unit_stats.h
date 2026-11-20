// A unit's stats as the rules see them: its own values plus what the
// accessories it carries and its side's finished technologies add (docs/ITEMS.md "Stats"). Free functions so the
// formula host can use them without a Battle.
#pragma once
#include "core/battle/battle.h"

namespace ote {

// The points the unit's rank derives from: its experience in the old mode, its merit counter with "rules.ranks". The ONE place that decides.
inline int rankPointsOf(const Database& db, const Unit& u) { return db.rules.unitRanks.merit ? u.merit : u.xp; }

// What the unit's accessories (and staves with a "bonus") add to one stat; two of the same item count once.
inline int itemBonus(const Database& db, const Unit& u, int statIndex) {
  int sum = 0;
  for (size_t i = 0; i < u.items.size(); ++i) {
    const ItemDef& d = db.items[(size_t)u.items[i].item];
    if ((d.kind != ItemKind::Accessory && d.kind != ItemKind::Staff) || (size_t)statIndex >= d.bonus.size()) continue;  // a staff may carry a bonus too
    bool earlier = false;
    for (size_t k = 0; k < i && !earlier; ++k) earlier = u.items[k].item == u.items[i].item;
    if (!earlier) sum += d.bonus[(size_t)statIndex];
  }
  return sum;
}

// What the unit's current job adds to one stat ("bonus" in data/jobs.json, docs/JOBS.md); 0 for a unit without a job and in a game without jobs.
inline int jobBonus(const Database& db, const Unit& u, int statIndex) {
  if (db.jobs.empty() || !u.jobs.on() || u.jobs.job >= (int)db.jobs.size() || statIndex < 0) return 0;
  const std::vector<int>& b = db.jobs[(size_t)u.jobs.job].bonus;
  return (size_t)statIndex < b.size() ? b[(size_t)statIndex] : 0;
}

// What the finished technologies of the unit's side add to one stat (docs/ECONOMY.md "Research"): every
// technology with a bonus whose "types" is empty or lists the unit's type.
inline int researchBonus(const Database& db, const SideState& side, const Unit& u, int statIndex) {
  int sum = 0;
  for (size_t t = 0; t < side.researched.size(); ++t) {
    if (!side.researched[t]) continue;
    const TechnologyDef& d = db.research[t];
    if ((size_t)statIndex >= d.bonus.size() || d.bonus[(size_t)statIndex] == 0) continue;
    bool applies = d.bonusTypes.empty();
    for (size_t k = 0; k < d.bonusTypes.size() && !applies; ++k) applies = d.bonusTypes[k] == u.type;
    if (applies) sum += d.bonus[(size_t)statIndex];
  }
  return sum;
}

// `sides` (optional) is the battle's side list, for the research bonus; without it only items count.
inline int unitStat(const Database& db, const Unit& u, int statIndex, const std::vector<SideState>* sides = nullptr) {
  int v = (size_t)statIndex < u.stats.size() ? u.stats[(size_t)statIndex] : 0;
  const bool research = sides && !db.research.empty() && u.side >= 0 && (size_t)u.side < sides->size();
  const bool ranked = db.rules.unitRanks.on();
  if (ranked) v += db.rules.unitRanks.bonusAt(rankPointsOf(db, u), statIndex);  // the unit's rank (docs/SRPG.md "Unit ranks")
  const int job = jobBonus(db, u, statIndex);
  v += job;
  if (u.items.empty() && !research) return (ranked || job != 0) && v < 0 ? 0 : v;
  v += itemBonus(db, u, statIndex);
  if (research) v += researchBonus(db, (*sides)[(size_t)u.side], u, statIndex);
  return v < 0 ? 0 : v;
}

}  // namespace ote
