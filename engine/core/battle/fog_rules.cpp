// What fog of war changes about moving (docs/ECONOMY.md "Fog of war").
//
// A move is PLANNED on what the unit's team sees: Battle::reachable treats an enemy it does not see as absent. The
// plan is fixed when the order is given, so at execution the truth can disagree: the unit walks the planned path
// square by square and stops in front of the first enemy unit it meets, seen or not (an ambush). Sight is not
// replanned during the walk: an enemy that comes into view a step before the unit reaches it still blocks, because
// the player only planned on what was seen at the start. The unit stops on the last square it can legally end on
// (a square with a friendly unit can be walked through but not stopped on, so it falls back further), spends fuel
// for the squares walked only, and its turn is over (Battle::applyMove).
#include "core/battle/battle.h"

namespace ote {

int Battle::pathStop(int unit, const std::vector<Cell>& path, int& blocker, Cell& blockedAt) const {
  blocker = -1;
  blockedAt = kNoCell;
  int stop = (int)path.size() - 1;
  if (!fog) return stop;  // without fog the planner never walks through an enemy
  const Unit& u = units[(size_t)unit];
  for (size_t i = 1; i < path.size(); ++i) {
    int occ = occupant[(size_t)path[i]];
    if (occ == -1 || occ == unit || !isEnemy(u, units[(size_t)occ])) continue;
    blocker = occ;
    blockedAt = path[i];
    stop = (int)i - 1;
    while (stop > 0 && occupant[(size_t)path[(size_t)stop]] != -1) --stop;  // never end on an ally's square
    return stop;
  }
  return stop;
}

void Battle::noticeAlong(int unit, const std::vector<Cell>& path, int upTo) {
  if (!fog || upTo < 2) return;
  Unit& u = units[(size_t)unit];
  const Cell here = u.cell;
  const int team = teamOf(u);
  // Squares 1 .. upTo-1: the unit stood on each of them for a moment. Its team's sight (and with it the last seen
  // owner of every structure it could see from there) is recomputed from each one; the destination is covered by the
  // normal recompute afterwards. Other teams' sight cannot change during a walk.
  for (int k = 1; k < upTo; ++k) {
    u.cell = path[(size_t)k];
    computeTeamSight(team);
  }
  u.cell = here;
  sight_.valid = false;  // the arrays now hold a mid-walk picture of this team
}

}  // namespace ote
