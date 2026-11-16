// Rescue and pair up in a battle (docs/SRPG.md "Rescue and pair up").
// STUB (step F1): no game has rules.rescue yet (the loader refuses the block), so nobody can be carried.
// Step R1 fills this file and pair_up.cpp.
#include "core/battle/battle.h"

namespace ote {

void Battle::rescueTargets(int, Cell, std::vector<int>& out) const { out.clear(); }
void Battle::dropCells(int, Cell, std::vector<Cell>& out) const { out.clear(); }
void Battle::giveTargets(int, Cell, std::vector<int>& out) const { out.clear(); }
void Battle::takeTargets(int, Cell, std::vector<int>& out) const { out.clear(); }
bool Battle::canSwitchPair(int) const { return false; }

bool Battle::validateRescue(const Action&, std::string* why) const {
  if (why) *why = "this game has no rescue or pair up";
  return false;
}

void Battle::applyRescue(const Action&, std::vector<Event>&) {}
int Battle::rescueStat(const Unit&, int, int value) const { return value; }
void Battle::rescueUnitGone(int, int, std::vector<Event>&) {}

}  // namespace ote
