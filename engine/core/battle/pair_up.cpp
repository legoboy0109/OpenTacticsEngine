// Pair up (docs/SRPG.md "Pair up"): SwitchPair and the query for it. PairUp and Drop are the shared actions of
// rescue.cpp (applyRescueAction / applyDrop); the pair bonus is in rescue.cpp's rescueStat.
#include "core/battle/battle.h"

namespace ote {

bool Battle::canSwitchPair(int unit) const {
  if (db->rules.rescue.mode != RescueMode::PairUp || !canAct(unit)) return false;
  const Unit& u = units[(size_t)unit];
  return !u.done && !u.acted && !switchWhy(u);
}

// The unit behind comes forward onto the front unit's square with its turn flags, so a pair moves once and acts
// once a turn whoever stands in front.
void Battle::applySwitchPair(const Action& action, std::vector<Event>& events) {
  const int front = action.unit;
  const int back = units[(size_t)front].carrying;
  const Cell square = units[(size_t)front].cell;
  markSight();
  releaseCapture(front, kNoCell);  // a capture belongs to the unit that was making it
  occupant[(size_t)square] = back;
  Unit& f = units[(size_t)front];
  Unit& b = units[(size_t)back];
  b.alive = true;
  b.cell = square;
  b.carriedBy = -1;
  b.carrying = front;
  b.moved = f.moved;
  b.acted = f.acted;
  b.moveSpent = f.moveSpent;
  b.refreshed = f.refreshed;  // "act again" already used by the pair stays used
  f.alive = false;
  f.cell = kNoCell;
  f.carrying = -1;
  f.carriedBy = back;
  Event e;
  e.type = EventType::PairSwitched;
  e.a = front;
  e.b = back;
  e.c = square;
  events.push_back(e);
}

}  // namespace ote
