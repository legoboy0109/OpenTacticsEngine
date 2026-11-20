// The words about a unit's rank for the unit details and the panel (docs/SRPG.md "Unit ranks"): plain functions of the battle, so a unit test can read them.
#pragma once
#include <string>

#include "core/battle/battle.h"

namespace ote {

// "Veteran" (the rank the unit holds; "No rank" before its first); and "XP 12/30" towards the next rank, or "XP 60 MAX" at the top. Used by the unit details and the panel.
std::string unitRankName(const Battle& b, const Unit& u);
// label false: "12/30" / "60 MAX" (the narrow panel row); `noMax` drops the " MAX" of the top rank (merit mode's panel row: "Merit 60").
std::string unitRankXpText(const Battle& b, const Unit& u, bool label = true, bool noMax = false);

}  // namespace ote
