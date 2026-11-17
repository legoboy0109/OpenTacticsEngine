// What the player says about changing a campaign's difficulty (docs/DIFFICULTY.md section 8), shared by the battle menu's
// Difficulty row and the game page's Difficulty row, so both follow the same rule and use the same wording.
#pragma once
#include <string>
#include <vector>

#include "core/data/difficulty.h"

namespace ote {

// The difficulties the player may move to from list position `now` under the game's "change" rule, in list order.
std::vector<int> difficultiesReachable(const DifficultyRules& rules, int now);

// The words after the current name when the row cannot be used: " (fixed)" (rule "never"), " (lowest)" (rule "lowerOnly" and
// already the easiest); "" when a change is possible (or nothing could be changed for another reason).
std::string difficultyBlockedNote(const DifficultyRules& rules, int now);

}  // namespace ote
