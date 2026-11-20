// A resource's "refill" (docs/ECONOMY.md "Refilling resources"): at the start of the owning side's turn its amount becomes
// min(what is left, carryOver) + perTurn. General: any resource may have it; the action-point system only happens to use it for its pool.
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

void Battle::refillTurnStart(std::vector<Event>& events) {
  for (size_t r = 0; r < db->resources.size(); ++r) {
    const ResourceDef& def = db->resources[r];
    if (!def.refill) continue;
    const int left = sides[(size_t)activeSide].resources[r];
    const int target = std::min(std::max(0, left), def.refillCarryOver) + def.refillPerTurn;
    changeResource(activeSide, (int)r, target - left, events);  // (nothing is said when it did not change)
  }
}

}  // namespace ote
