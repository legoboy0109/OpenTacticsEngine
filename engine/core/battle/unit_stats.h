// A unit's stats as the rules see them: its own values plus what the
// accessories it carries add (docs/ITEMS.md "Stats"). Free functions so the
// formula host can use them without a Battle.
#pragma once
#include "core/battle/battle.h"

namespace ote {

// What the unit's accessories add to one stat; two of the same item count once.
inline int itemBonus(const Database& db, const Unit& u, int statIndex) {
  int sum = 0;
  for (size_t i = 0; i < u.items.size(); ++i) {
    const ItemDef& d = db.items[(size_t)u.items[i].item];
    if (d.kind != ItemKind::Accessory || (size_t)statIndex >= d.bonus.size()) continue;
    bool earlier = false;
    for (size_t k = 0; k < i && !earlier; ++k) earlier = u.items[k].item == u.items[i].item;
    if (!earlier) sum += d.bonus[(size_t)statIndex];
  }
  return sum;
}

inline int unitStat(const Database& db, const Unit& u, int statIndex) {
  int v = (size_t)statIndex < u.stats.size() ? u.stats[(size_t)statIndex] : 0;
  if (u.items.empty()) return v;
  v += itemBonus(db, u, statIndex);
  return v < 0 ? 0 : v;
}

}  // namespace ote
