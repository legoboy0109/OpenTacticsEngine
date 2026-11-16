// Supports in a battle (docs/SRPG.md "Supports").
// STUB (step F1): no game has support pairs yet (data/supports.json is refused by the loader), so Battle::supports
// stays empty, nothing earns points and there is no conversation to have. Step S1 fills this file.
#include "core/battle/battle.h"

namespace ote {

void Battle::supportOptions(int, Cell, std::vector<int>& out) const { out.clear(); }

void Battle::setStartingSupports(const SupportState& carried) {
  // Only a state that fits the game's pairs is taken (a game without pairs: only the empty one).
  if (carried.points.size() != db->supports.pairs.size() || carried.level.size() != db->supports.pairs.size()) return;
  supports = carried;
}

void Battle::supportsInit() {}

bool Battle::validateSupport(const Action&, std::string* why) const {
  if (why) *why = "this game has no supports";
  return false;
}

void Battle::applySupport(const Action&, std::vector<Event>&) {}
void Battle::supportsTurnEnd(std::vector<Event>&) {}
void Battle::supportsAfterCombat(int, int, std::vector<Event>&) {}
void Battle::supportsHealed(int, int, std::vector<Event>&) {}
void Battle::supportCombatMods(const CombatSetup&, CombatMods&, CombatMods&) const {}

// The story loader refuses the command and the condition until supports exist, so these are never reached.
void Battle::storyAddSupport(const Command&, std::vector<Event>&) {}
bool Battle::storySupportHolds(const Condition&) const { return false; }

}  // namespace ote
