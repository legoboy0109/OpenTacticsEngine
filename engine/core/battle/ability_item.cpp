// An ability that spends its item (docs/SRPG.md "An ability that spends its item"): which carried item gives a unit an
// ability, and taking one of its uses when the ability is used. The grant itself is the ordinary one (a skill with an
// "ability" effect in an "always" block, on an accessory or on the weapon in use); only the bookkeeping is new.
#include "core/battle/battle.h"
#include "core/data/ability_grants.h"

namespace ote {

int Battle::abilityItemSlot(const Unit& u, int ability) const {
  // Accessories in inventory order, then the weapon in use: the first that gives it is the one that is spent.
  for (size_t i = 0; i < u.items.size(); ++i)
    if (itemOf(u.items[i]).kind == ItemKind::Accessory && itemGivesAbility(*db, u.items[i].item, ability)) return (int)i;
  if (db->rules.items.carriedWeapons) {
    const int slot = equippedSlot(u);
    if (slot >= 0 && itemGivesAbility(*db, u.items[(size_t)slot].item, ability)) return slot;
  }
  return -1;
}

void Battle::spendAbilityItem(int unit, int ability, std::vector<Event>& events) {
  if (!db->abilities[(size_t)ability].spendsItem) return;
  const int slot = abilityItemSlot(units[(size_t)unit], ability);
  if (slot >= 0) spendUse(unit, slot, events);  // an unlimited item spends nothing; at 0 it breaks (ItemBroke) as any item does
}

}  // namespace ote
