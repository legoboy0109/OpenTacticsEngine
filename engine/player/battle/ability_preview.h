// What an ability would do on the board, for the targeting mode's marks (docs/SRPG.md step U3): the squares the
// units end on (push, pull, swap, moveBehind). Pure: no scene, no drawing. Worked out like the core's plan
// (ability_moves.cpp) but on what the user's team sees, so a hidden unit never shows up in a preview: a square
// held by a unit the team cannot see counts as free here (and the use is then "blocked" when it plays).
#pragma once
#include <vector>

#include "core/battle/battle.h"

namespace ote {

struct AbilityMovePreview {
  int unit = -1;
  std::vector<Cell> path;  // squares it passes; the last one is where it ends (a push can be several)
};

// Empty for heal and summon (nothing moves) and when nothing would move.
std::vector<AbilityMovePreview> previewAbilityMoves(const Battle& b, int user, int ability, int target);

}  // namespace ote
