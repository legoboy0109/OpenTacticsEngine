// Computer opponent: looting and escaping (docs/AI.md "Looting and escaping").
//
//   updateLooters  at the start of every AI step: a unit in mode "loot" that has nothing left it could take anywhere it can walk to
//                  (lootLeft, ai_places.cpp) starts escaping, for good (Unit::escaping, saved). Taking things is the ordinary goesFor
//                  play of ai_places.cpp / ai_abilities.cpp; the mode only adds this end, and that it never walks toward the enemy.
//   escapeOrder    an escaping unit (mode "escape", or a looter that is done) never attacks: it leaves by the exit it stands on, else walks
//                  to the nearest of the map's exits (and leaves at once when it can stop on one this turn), else, with no exit it can
//                  reach, keeps away from the enemy like a retreating unit.
#include <algorithm>

#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

void updateLooters(Battle& b) {
  for (const Unit& u : b.units) {
    if (!u.alive || u.side != b.activeSide || u.done || u.behaviour.mode != AiMode::Loot || u.escaping) continue;
    if (b.round < u.behaviour.fromTurn) continue;  // not started yet: it has not looked for anything
    if (!lootLeft(b, u)) b.startEscaping(u.id);
  }
}

bool escapesNow(const Unit& u) { return u.behaviour.mode == AiMode::Escape || (u.behaviour.mode == AiMode::Loot && u.escaping); }

Order escapeOrder(Context& cx, const Unit& u, const std::vector<Cell>& stops) {
  const Battle& b = cx.b;
  Order order;  // Wait in place
  if (b.isExit(u.cell)) {
    order.kind = ActionType::LeaveMap;
    order.score = 150;
    return order;
  }
  const int mt = b.typeOf(u).moveType;
  std::vector<Cell> goals;
  for (Cell c : b.exits)
    if (b.enterCost(c, mt) > 0) goals.push_back(c);
  std::vector<int> field;
  if (!goals.empty()) distanceField(b, mt, goals, field);
  if (goals.empty() || field[(size_t)u.cell] >= kFar) {  // no exit it can ever get to: away from the enemy, as a retreating unit does
    const Cell safe = stops.size() < 2 ? kNoCell : safestStop(b, u, stops);
    if (safe != kNoCell) {
      order.moveTo = safe;
      order.score = 100;
    }
    return order;
  }
  // An exit it can stop on this turn: there, and out (with several, the first in the move search's order: deterministic).
  for (Cell s : stops)
    if (s != u.cell && b.isExit(s)) {
      order.moveTo = s;
      order.kind = ActionType::LeaveMap;
      order.score = 150;
      return order;
    }
  int bestVal = 0;
  const Cell step = stops.size() < 2 ? kNoCell : stepToward(b, u, field, stops, bestVal);
  if (step != kNoCell) {
    order.moveTo = step;
    order.score = 100 - std::min(bestVal, 99);
  }
  return order;
}

}  // namespace ai
}  // namespace ote
