// What a strike ability would do, for the player's pick (docs/SRPG.md "Abilities that strike"): the forecast line the hint shows while a target is
// highlighted, and for an area weapon the squares it covers. Plain text over the core's forecasts (Battle::abilityStrikeForecast / abilityStrikeArea).
#pragma once
#include <string>
#include <vector>

#include "core/battle/battle.h"

namespace ote {

// "Fire: 14 dmg, 85% hit, 5% crit" for one foe (x2 when it strikes twice); "" when it cannot strike it.
std::string strikeUnitLine(const Battle& b, int user, int ability, int target);
// "Blaze: 3 hit (Goblin 12 dmg 90%, ...)" for an area aimed at `cell`; `covered` = every square it covers. "" when it cannot aim there.
std::string strikeAreaLine(const Battle& b, int user, int ability, Cell cell, std::vector<Cell>& covered);

}  // namespace ote
