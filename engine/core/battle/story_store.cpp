// The storeItems command (docs/EVENTS.md "Sending a unit's items to storage"): a unit's items go to its side's shared storage.
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

// Carried items move one by one in the unit's own order, so the order in the storage is the order the unit carried them. A computer
// side's units are left alone (its "storage" is not a thing a story or a player ever reaches). The storage has no size limit in this
// engine, so nothing is ever refused for room. A game without storage is refused when the game loads (loader_story_store.cpp).
void Battle::storyStoreItems(const Command& c, std::vector<Event>& events) {
  if (db->rules.items.storage == ItemStorage::Off) return;  // the loader refuses this; kept as a guard for hand-built data
  const int count = (int)units.size();
  for (int i = 0; i < count; ++i) {
    if (!exists(units[(size_t)i]) || !unitMatches(c.filter, units[(size_t)i])) continue;
    const int side = units[(size_t)i].side;
    if (sides[(size_t)side].ai) continue;
    std::vector<ItemStack>& items = units[(size_t)i].items;
    bool moved = false;
    for (size_t s = 0; s < items.size();) {
      if (std::find(c.keep.begin(), c.keep.end(), items[s].item) != c.keep.end()) { ++s; continue; }
      ItemStack st = items[s];
      st.drop = false;
      items.erase(items.begin() + (long)s);
      sides[(size_t)side].storage.push_back(st);
      Event e;
      e.type = EventType::ItemMoved;  // the same entry as the Store action: from this unit to the storage (-1)
      e.a = i;
      e.b = -1;
      e.c = st.item;
      e.d = side;
      events.push_back(e);
      moved = true;
    }
    if (moved) clampHp(units[(size_t)i]);  // an accessory that raised the maximum is gone
  }
}

}  // namespace ote
