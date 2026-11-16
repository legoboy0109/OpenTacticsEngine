// The hooks of the strategy-RPG features (docs/SRPG.md) that the shared battle code calls, and the fixed order
// in which each hook asks the areas. The areas' own parts live in their files (characters.cpp, skills.cpp,
// promotion.cpp, abilities.cpp, rescue.cpp, supports.cpp) and start as stubs that do nothing, so a game that
// uses none of the features plays, saves and rolls exactly as before.
#include "core/battle/battle.h"

namespace ote {

int Battle::srpgStat(const Unit& u, int stat, int base) const {
  // Nothing can apply: hand back the value untouched (not even clamped: exactly what the rules saw before).
  if (db->skills.empty() && db->rules.rescue.mode == RescueMode::Off) return base;
  int v = base + skillStatBonus(u, stat);
  v = rescueStat(u, stat, v);  // carry penalty (percent, then the added number) / pair bonus
  return v < 0 ? 0 : v;
}

void Battle::dressUnit(Unit& u) const {
  characterDress(u);  // first: the skills learned by its level include the character's
  rankRaiseToClass(u, u.id, nullptr);  // weapon ranks its class and character give (silent; docs/SRPG.md section 7)
  learnUpTo(u);
}

void Battle::endOrder(int unit) {
  Unit& u = units[(size_t)unit];
  // A skill gave this unit "act again" during the combat that is ending: its flags were cleared on purpose.
  if (actAgainPending_ == unit) {
    actAgainPending_ = -1;
    return;
  }
  // "Move again": the action is used up but the movement that is left may still be walked.
  if (u.alive && hasMoveAgain(u) && u.moveSpent < movePoints(u)) u.acted = true;
  else u.done = true;
}

void Battle::srpgTurnStart(std::vector<Event>& events) {
  abilitiesTurnStart(events);  // waits and lifetimes first: a summon whose time is up leaves before anything heals it
  promotionTurnStart(events);  // then promotions, so a promoted unit's new skills count below
  skillsTurnStart(events);
}

void Battle::srpgTurnEnd(std::vector<Event>& events) { supportsTurnEnd(events); }

void Battle::srpgUnitGone(int unit, int by, std::vector<Event>& events) {
  if (db->abilities.empty() && db->rules.rescue.mode == RescueMode::Off) return;
  rescueUnitGone(unit, by, events);  // first: the unit it carried may take the square it left
  summonsUnitGone(unit, events);
}

void Battle::srpgLevelUp(int unit, std::vector<Event>& events) {
  skillsLevelUp(unit, events);
  promotionLevelUp(unit, events);
}

void Battle::srpgAfterCombat(int attacker, int defender, int hpA, int hpD, std::vector<Event>& events) {
  skillsAfterCombat(attacker, defender, hpA, hpD, events);
  supportsAfterCombat(attacker, defender, events);
}

}  // namespace ote
