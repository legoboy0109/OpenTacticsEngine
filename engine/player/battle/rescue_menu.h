// What the action menu offers a unit for carrying / pair up, and the text of the info panel row (docs/SRPG.md
// "Player UI", step U4). Free of SDL and of the scene so a unit test can drive it with a Battle alone.
#pragma once
#include <string>
#include <vector>

#include "core/battle/battle.h"

namespace ote {

struct RescueEntry {
  std::string label, extra;  // extra = the reason a greyed entry is greyed (empty otherwise)
  bool enabled = true;
  int id = 0;                // SrpgMenuId: kRescue .. kSwitch
};

// The entries of `unit` standing where it stands, in the contract's order: carry = Rescue, Drop, Give, Take;
// pair up = Pair up, Switch, Drop. Each only when it applies; Drop of a unit that has nowhere to put the other
// one down is shown greyed with "no room". Empty in a game without rules.rescue and for a unit that has acted.
std::vector<RescueEntry> rescueEntries(const Battle& b, int unit);

// The candidates of the picks behind the entries (units for Rescue / Give / Take / Pair up, squares for Drop).
std::vector<int> rescuePickUnits(const Battle& b, int unit, int menuId);
std::vector<Cell> rescuePickCells(const Battle& b, int unit);

// The info panel row of a unit that has someone: "Carrying: Bram" (carry) or "With: Bram" (pair up); empty when
// it has nobody. `hideName` (fog, a unit of another team) gives "Carrying: someone": the name is not the
// viewer's business, that the unit carries somebody is on show anyway (the marker).
std::string rescueRowText(const Battle& b, const Unit& u, bool hideName);

}  // namespace ote
