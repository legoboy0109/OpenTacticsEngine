// The abilities that move units (docs/SRPG.md "Directions" and "Legality"): push, pull, swap, moveBehind, movePast.
// All of them think in directions through Grid (directionTo, neighbor, opposite), never in coordinates, so one
// piece of code serves the square grid and both hexagon layouts.
#include "core/battle/battle.h"

namespace ote {

// Worked out on the truth, from the user's own square, before anything changes. The target comes first in `moves`
// (the events and the story's "reach" events follow that order). A move that the truth forbids (an enemy nobody on
// the team saw stands on the square) leaves `blocker` set and, for pull / moveBehind, nothing moves.
void Battle::planAbilityMove(const Unit& user, const AbilityDef& def, int target, AbilityPlan& out) const {
  out = AbilityPlan();
  const Unit& t = units[(size_t)target];
  const int dir = grid.directionTo(user.cell, t.cell);
  const Cell back = grid.neighbor(user.cell, grid.opposite(dir));
  switch (def.effect) {
    case AbilityEffect::Push: {
      // Up to `amount` squares straight away from the user; it stops before the first square it cannot take.
      Cell at = t.cell;
      for (int step = 0; step < def.amount; ++step) {
        const Cell next = grid.neighbor(at, dir);
        if (!grid.valid(next) || enterCost(next, typeOf(t).moveType) <= 0) break;
        if (occupant[(size_t)next] >= 0) {
          out.blocker = occupant[(size_t)next];
          break;
        }
        at = next;
      }
      if (at != t.cell) out.moves.push_back({target, at});
      break;
    }
    case AbilityEffect::Pull:
      // The user steps back and the target takes the square it left.
      if (occupant[(size_t)back] >= 0) out.blocker = occupant[(size_t)back];
      else out.moves = {{target, user.cell}, {user.id, back}};
      break;
    case AbilityEffect::Swap:
      out.moves = {{target, user.cell}, {user.id, t.cell}};
      break;
    case AbilityEffect::MoveBehind:
      if (occupant[(size_t)back] >= 0) out.blocker = occupant[(size_t)back];
      else out.moves = {{target, back}};
      break;
    case AbilityEffect::MovePast: {
      // The user steps to the square on the far side of the target, along the same line.
      const Cell far = grid.neighbor(t.cell, dir);
      if (!grid.valid(far) || enterCost(far, typeOf(user).moveType) <= 0) break;  // cannot happen after validation
      if (occupant[(size_t)far] >= 0) out.blocker = occupant[(size_t)far];
      else out.moves = {{user.id, far}};
      break;
    }
    default: break;
  }
}

// Puts every unit of the plan on its square at once (a swap needs both squares emptied first), then reports them.
void Battle::doAbilityMoves(const AbilityPlan& plan, std::vector<Event>& events) {
  if (plan.moves.empty()) return;
  std::vector<Cell> from;
  for (const AbilityMove& m : plan.moves) {
    from.push_back(units[(size_t)m.unit].cell);
    releaseCapture(m.unit, m.to);  // leaving a structure it was capturing abandons the capture
  }
  markSight();  // the events so far (AbilityUsed) are stamped with the sight from before anyone moved
  for (const Cell c : from) occupant[(size_t)c] = -1;
  for (const AbilityMove& m : plan.moves) {
    occupant[(size_t)m.to] = m.unit;
    units[(size_t)m.unit].cell = m.to;
  }
  for (size_t i = 0; i < plan.moves.size(); ++i) {
    Event e;
    e.type = EventType::UnitRelocated;
    e.a = plan.moves[i].unit;
    e.b = from[i];
    e.c = plan.moves[i].to;
    events.push_back(e);
  }
}

}  // namespace ote
