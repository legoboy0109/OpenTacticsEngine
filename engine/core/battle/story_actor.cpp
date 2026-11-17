// The `actor` condition (docs/EVENTS.md "Branching on who acted"): the unit that set the running event off.
#include "core/battle/battle.h"

namespace ote {

int Battle::storyActorOf(const StoryOccurrence& o) const {
  using K = Trigger::Kind;
  switch (o.kind) {
    // Reach: who moved; Defeated: who fell; Fight: the attacker; Promoted: who promoted; Place: who opened / visited / destroyed
    // it (-1 when the story did). Turn, battle and support events have nobody acting. (A Talk event's actor is the talking unit: it is
    // queued with that unit, see applyTalk.)
    case K::Reach: case K::Defeated: case K::Fight: case K::Promoted: case K::Place:
      return o.unit;
    default:
      return -1;
  }
}

bool Battle::storyActorHolds(const Condition& c, int actor) const {
  return actor >= 0 && actor < (int)units.size() && unitMatches(c.filter, units[(size_t)actor]);
}

}  // namespace ote
