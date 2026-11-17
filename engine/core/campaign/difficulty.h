// A playthrough's difficulty (docs/DIFFICULTY.md): chosen at New Game, changed later only as the game's "change" rule allows.
#pragma once
#include <string>

#include "core/campaign/playthrough.h"

namespace ote {

// The difficulty of the playthrough as a list position (-1 in a game without difficulties).
int playthroughDifficulty(const Database& db, const Playthrough& pt);
// Sets it (New Game: `choosing` true = any id of the list; later: only what DifficultyRules::mayChange allows). False
// with `why` (plain words for the player) when the id is unknown or the game's rule does not allow it. A change takes
// effect from the next battle built; a battle in progress is never rewritten.
bool setPlaythroughDifficulty(const Database& db, Playthrough& pt, const std::string& id, bool choosing, std::string& why);

}  // namespace ote
