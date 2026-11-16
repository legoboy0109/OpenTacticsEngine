// Timed skill blocks (docs/SRPG.md "Skills": turnStart, defeat, afterCombat, actAgain).
// STUB (step K1 left these for step K2, which owns this file): nothing happens yet.
#include "core/battle/battle.h"

namespace ote {

void Battle::skillsTurnStart(std::vector<Event>&) {}
void Battle::skillsAfterCombat(int, int, int, int, std::vector<Event>&) {}

}  // namespace ote
