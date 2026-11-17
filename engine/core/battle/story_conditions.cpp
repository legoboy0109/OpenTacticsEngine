// Status conditions in the story (docs/SRPG.md section 9, docs/EVENTS.md "Status conditions"): addCondition, removeCondition and the
// condition hasCondition. They go through the same inflictCondition / cureConditions as a staff, a weapon or an item, so immunity,
// "replaces" and the events (ConditionApplied / ConditionResisted / ConditionEnded) are exactly the ones every other route gives.
#include "core/battle/battle.h"

namespace ote {

// addCondition: every living unit matching the filter gets the condition (an immune unit resists: ConditionResisted). `c.value` is the
// optional number of turns (0 = the condition's own). The unit that did it is -1 (nobody: the story).
void Battle::storyAddCondition(const Command& c, std::vector<Event>& events) {
  const int count = (int)units.size();
  for (int i = 0; i < count; ++i) {
    const Unit& u = units[(size_t)i];
    if (!u.alive || !unitMatches(c.filter, u)) continue;
    inflictCondition(i, c.index, c.value, -1, events);
  }
}

// removeCondition: the named condition (`c.on` = every condition) goes from every living unit matching the filter; reason "cured".
void Battle::storyRemoveCondition(const Command& c, std::vector<Event>& events) {
  const int count = (int)units.size();
  for (int i = 0; i < count; ++i) {
    const Unit& u = units[(size_t)i];
    if (!u.alive || !unitMatches(c.filter, u)) continue;
    cureConditions(i, {c.index}, c.on, kConditionCured, events);
  }
}

bool Battle::storyHasCondition(const Condition& c) const {
  for (const Unit& u : units)
    if (u.alive && unitMatches(c.filter, u) && hasCondition(u, c.index)) return true;
  return false;
}

}  // namespace ote
