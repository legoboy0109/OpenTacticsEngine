// What an area ability would do, for the player's pick (docs/SRPG.md "Areas and several effects for abilities"): the hint line while a square is
// highlighted, and the squares the area covers. Plain text over Battle::abilityAreaForecast; placeholder wording like the rest of the battle
// menus. Definitions: unit_info_ability_area.cpp (named unit_info_* so the unit tests' source glob picks it up).
#pragma once
#include <string>
#include <vector>

#include "core/battle/battle.h"

namespace ote {

// "Cura: 3 units (Rook +12 HP, Mia +8 HP cured, ...)" for the area aimed at `cell`; `covered` = every square it covers. "" when it cannot be
// aimed there. Units hidden by fog are never named or counted.
std::string abilityAreaLine(const Battle& b, int user, int ability, Cell cell, std::vector<Cell>& covered);

}  // namespace ote
