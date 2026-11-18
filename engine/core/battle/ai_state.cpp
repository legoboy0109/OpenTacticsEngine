// Computer opponent: the state the scripted-style behaviour keys keep (docs/AI.md "Scripted-style computer players"), updated at the
// start of each of the side's turns, after healing and damage. A unit whose behaviour has none of the keys is not touched.
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

void Battle::aiTurnStart() {
  for (Unit& u : units) {
    if (!u.alive || u.side != activeSide || u.behaviour.isDefault()) continue;
    const AiBehaviour& beh = u.behaviour;
    if (beh.idleTurns > 0) ++u.idleSeen;  // its own turns on the map, from the one it got the key in: idle while this is <= idleTurns
    if (beh.retreat.on) {
      const int full = std::max(1, maxHp(u));
      if (!u.retreating && u.hp * 100 < beh.retreat.below * full) u.retreating = true;
      else if (u.retreating && u.hp * 100 >= beh.retreat.until * full) u.retreating = false;
    }
    if (beh.mode == AiMode::Patrol && !beh.route.empty()) {
      const int n = (int)beh.route.size();
      if (u.patrolAt < 0 || u.patrolAt >= n) u.patrolAt = 0;
      const std::pair<int, int>& goal = beh.route[(size_t)u.patrolAt];
      if (goal.first < grid.width() && goal.second < grid.height()) {
        const Cell c = grid.at(goal.first, goal.second);
        // Reached: standing on it, or next to it while another unit stands on it (it cannot get in, and must not wait forever).
        const bool blocked = occupant[(size_t)c] >= 0 && occupant[(size_t)c] != u.id && grid.distance(u.cell, c) == 1;
        if (u.cell == c || blocked) u.patrolAt = (u.patrolAt + 1) % n;
      }
    }
  }
}

}  // namespace ote
