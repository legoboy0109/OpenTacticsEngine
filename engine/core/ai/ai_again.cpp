// "Move again" (docs/SRPG.md "Turn flags"): a unit whose action is used up (`Unit::acted`) may still walk the movement
// it has left. The AI uses that to step away from the enemies that could reach it; otherwise it waits.
// Nothing here can loop: the move ends the unit's turn (`Battle::apply` of a Move sets `done` for an acted unit).
#include <algorithm>

#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

namespace {

// Known enemies that could hit `c` next turn (walk plus longest weapon, by distance as the rest of the AI judges it).
int threatsAt(const Battle& b, const Unit& u, Cell c) {
  int n = 0;
  std::vector<int> weapons;
  for (const Unit& o : b.units) {
    if (!o.alive || !b.isEnemy(u, o) || !knownEnemy(b, u.side, o)) continue;
    int range = 1;
    b.weaponsOf(o, weapons);
    for (int w : weapons) range = std::max(range, b.db->weapons[(size_t)w].maxRange);
    if (b.grid.distance(c, o.cell) <= b.movePoints(o) + range) ++n;
  }
  return n;
}

}  // namespace

Order moveAgainOrder(const Battle& b, const Unit& u, const std::vector<Cell>& stops) {
  Order order;  // Wait in place
  int bestThreat = threatsAt(b, u, u.cell);
  double bestDef = terrainDefense(b, u.cell);
  Cell bestCell = kNoCell;
  for (Cell s : stops) {
    if (s == u.cell) continue;
    int t = threatsAt(b, u, s);
    double def = terrainDefense(b, s);
    // Strictly better only: fewer threats, or as few and better cover. Equal squares are not worth a walk.
    if (t < bestThreat || (t == bestThreat && def > bestDef)) {
      bestThreat = t;
      bestDef = def;
      bestCell = s;
    }
  }
  if (bestCell != kNoCell) {
    order.moveTo = bestCell;
    order.score = 1;
  }
  return order;
}

}  // namespace ai
}  // namespace ote
