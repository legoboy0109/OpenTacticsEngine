// Computer opponent: retreat to heal (docs/AI.md "Retreat to heal"). Whether the unit is retreating (Unit::retreating) is decided at the start of
// its turn (Battle::aiTurnStart); this file makes the order of a unit that is. It never attacks: it drinks a healing item it carries (if
// "useItems"), else walks toward the nearest free square of the terrain it falls back to (or, with none to go to, to the reachable square
// where the fewest known enemies can hit it) and waits.
#include <algorithm>

#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

namespace {

// Known enemies that could hit `c` next turn: their walk plus their longest weapon, by distance as the rest of the AI judges it.
void threatsAt(const Battle& b, const Unit& u, Cell c, int& count, int& nearest) {
  count = 0;
  nearest = kFar;
  std::vector<int> weapons;
  for (const Unit& o : b.units) {
    if (!o.alive || !b.isEnemy(u, o) || !knownEnemy(b, u.side, o)) continue;
    int range = 1;
    b.weaponsOf(o, weapons);
    for (int w : weapons) range = std::max(range, b.db->weapons[(size_t)w].maxRange);
    const int d = b.grid.distance(c, o.cell);
    if (d <= b.movePoints(o) + range) ++count;
    nearest = std::min(nearest, d);
  }
}

}  // namespace

Order retreatOrder(Context& cx, const Unit& u, const std::vector<Cell>& stops) {
  const Battle& b = cx.b;
  const AiRetreat& r = u.behaviour.retreat;
  Order order;  // Wait in place
  if (r.useItems) {
    const int slot = healingItemSlot(b, u);
    if (slot >= 0) {
      order.score = 150;
      order.kind = ActionType::UseItem;
      order.slot = slot;
      order.target = u.id;
      return order;
    }
  }
  if (stops.size() < 2) return order;  // holds its square (or boxed in): it just waits

  // Toward the terrain: the field's goals are the free squares of the listed terrain the unit can enter.
  if (!r.terrains.empty()) {
    const int mt = b.typeOf(u).moveType;
    std::vector<Cell> goals;
    for (Cell c = 0; c < (Cell)b.grid.cellCount(); ++c) {
      if (std::find(r.terrains.begin(), r.terrains.end(), b.cells[(size_t)c].terrain) == r.terrains.end()) continue;
      if (b.enterCost(c, mt) <= 0 || (b.unitAt(c) >= 0 && b.unitAt(c) != u.id)) continue;
      goals.push_back(c);
    }
    std::vector<int> field;
    if (!goals.empty()) distanceField(b, mt, goals, field);
    if (!goals.empty() && field[(size_t)u.cell] < kFar) {
      int bestVal = 0;
      const Cell bestCell = stepToward(b, u, field, stops, bestVal);
      if (bestCell != kNoCell) {
        order.moveTo = bestCell;
        order.score = 100 - std::min(bestVal, 99);
      }
      return order;  // (nothing nearer: it waits where it is, on the terrain or as close as it can get)
    }
    // No such square it can get to at all: away from the enemy instead.
  }

  const Cell bestCell = safestStop(b, u, stops);
  if (bestCell != kNoCell) {
    order.moveTo = bestCell;
    order.score = 100;
  }
  return order;
}

Cell stepToward(const Battle& b, const Unit& u, const std::vector<int>& field, const std::vector<Cell>& stops, int& bestVal) {
  bestVal = field[(size_t)u.cell];
  Cell bestCell = kNoCell;
  double bestDef = -1;
  for (Cell s : stops) {
    if (s == u.cell || field[(size_t)s] >= kFar) continue;
    const int val = field[(size_t)s];
    const double def = terrainDefense(b, s);
    // The least remaining cost; equal: the better terrain, then the square found first in the move search (cheapest walk, lowest square).
    if (val < bestVal || (val == bestVal && bestCell != kNoCell && def > bestDef)) {
      bestVal = val;
      bestCell = s;
      bestDef = def;
    }
  }
  return bestCell;
}

Cell safestStop(const Battle& b, const Unit& u, const std::vector<Cell>& stops) {
  int bestCount, bestNearest;
  threatsAt(b, u, u.cell, bestCount, bestNearest);
  double bestDef = terrainDefense(b, u.cell);
  Cell bestCell = kNoCell;
  for (Cell s : stops) {
    if (s == u.cell) continue;
    int count, nearest;
    threatsAt(b, u, s, count, nearest);
    const double def = terrainDefense(b, s);
    // Fewest enemies that can hit it; equal: farther from the nearest one; equal: better terrain; equal: the first found (strictly better only).
    const bool better = count != bestCount ? count < bestCount : nearest != bestNearest ? nearest > bestNearest : def > bestDef;
    if (better) {
      bestCount = count;
      bestNearest = nearest;
      bestDef = def;
      bestCell = s;
    }
  }
  return bestCell;
}

}  // namespace ai
}  // namespace ote
