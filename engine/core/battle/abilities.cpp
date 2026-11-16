// Menu abilities and summons in a battle (docs/SRPG.md "Menu abilities and summoning").
// STUB (step F1): no game has abilities yet (data/abilities.json is refused by the loader), so no unit has one.
// Step A1 fills this file and its siblings (ability_moves.cpp, summons.cpp).
#include "core/battle/battle.h"

namespace ote {

void Battle::abilitiesOf(const Unit&, std::vector<int>& out) const { out.clear(); }
void Battle::abilityOptions(int, Cell, std::vector<AbilityOption>& out) const { out.clear(); }

void Battle::abilityTargets(int, int, Cell, std::vector<int>& units_, std::vector<Cell>& cells_) const {
  units_.clear();
  cells_.clear();
}

bool Battle::validateAbility(const Action&, std::string* why) const {
  if (why) *why = "this game has no abilities";
  return false;
}

void Battle::applyAbility(const Action&, std::vector<Event>&) {}
void Battle::abilitiesTurnStart(std::vector<Event>&) {}
void Battle::summonsUnitGone(int, std::vector<Event>&) {}

}  // namespace ote
