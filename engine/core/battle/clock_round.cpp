// Rounds of the speed turn order (docs/TURN_ORDER.md "Rounds"): a round passes every SpeedRules::roundTicks ticks of the clock. What the usual
// game does when a SIDE's turn starts (light torches burning down, refills, income, the research count-down, the story's turn events) happens
// here, once per round for every side still in the battle, in side order. What belongs to a unit (condition counts, healing terrain, fuel,
// ability waits) happens when that unit's own turn begins (clock.cpp).
#include "core/battle/battle.h"

namespace ote {

void Battle::beginSideRound(int side, std::vector<Event>& events) {
  activeSide = side;  // the per-side rules read the active side; the next unit's turn sets it right again
  lightsTurnStart();
  sideTurnEffects(events);
  StoryOccurrence o;
  o.kind = Trigger::Kind::Turn;
  o.side = side;
  o.round = round;
  fireStory(o, events);
}

// A stopped unit never gets a turn, so its conditions would never count down. They count with the rounds instead: once per round it is stopped,
// as if it had a turn that did nothing. (A condition that does not stop it counts with the unit's own turns.)
void Battle::tickStoppedConditions(std::vector<Event>& events) {
  if (db->conditions.empty()) return;
  for (size_t i = 0; i < units.size() && !over; ++i) {
    const Unit& u = units[i];
    if (!inClock(u) || u.conditions.empty() || conditionBlock(u, ConditionBlock::Act) < 0) continue;
    activeUnit = (int)i;  // the condition rules look at the unit that has the turn
    conditionsTurnStart(events);
    conditionsTurnEnd(events);
    activeUnit = -1;
  }
}

void Battle::beginRound(std::vector<Event>& events) {
  Event e;
  e.type = EventType::RoundStarted;
  e.a = round;
  events.push_back(e);
  const int keep = activeSide;
  for (int s = 0; s < (int)sides.size() && !over; ++s)
    if (!sides[(size_t)s].defeated) beginSideRound(s, events);
  if (!over) tickStoppedConditions(events);
  activeSide = keep;
}

}  // namespace ote
