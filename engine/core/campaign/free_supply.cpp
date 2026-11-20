#include "core/campaign/free_supply.h"

#include <algorithm>

namespace ote {

std::vector<ItemGrant> freeBattleSupply(const Database& db) {
  std::vector<ItemGrant> out;
  const FreeBattleStock& s = db.rules.items.freeStock;
  if (!s.given || db.rules.items.storage == ItemStorage::Off) return out;
  for (size_t i = 0; i < db.items.size(); ++i) {
    int count = s.each;
    ItemGrant g;
    g.item = (int)i;
    g.uses = db.items[i].uses;
    if (std::find(s.except.begin(), s.except.end(), (int)i) != s.except.end()) count = 0;
    for (const FreeStockCount& n : s.counts)
      if (n.grant.item == (int)i) {
        count = n.count;
        g.uses = n.grant.uses;
      }
    for (int k = 0; k < count; ++k) out.push_back(g);  // identical copies stay together, in the game's item order
  }
  return out;
}

void applyFreeSupply(const Database& db, MapDef& map) {
  const FreeBattleStock& s = db.rules.items.freeStock;
  if (!s.given) return;
  const std::vector<ItemGrant> supply = freeBattleSupply(db);
  for (MapSideDef& side : map.sides) {
    if (side.ai) continue;
    side.storage.insert(side.storage.end(), supply.begin(), supply.end());
    for (const Amount& a : s.funds) {
      auto it = std::find_if(side.resources.begin(), side.resources.end(), [&](const Amount& r) { return r.resource == a.resource; });
      if (it != side.resources.end()) it->amount += a.amount;
      else side.resources.push_back(a);
    }
  }
}

}  // namespace ote
