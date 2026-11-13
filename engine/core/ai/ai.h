// Computer opponent. Works only through Battle's public queries and
// apply(), exactly like a human player would, so it needs no changes when
// rules modules are added (it just sees more/less available actions).
#pragma once
#include "core/battle/battle.h"

namespace ote {

// Performs ONE decision for the active side and applies it: a unit's whole
// order (optional Move, then Attack/Capture/Wait), one Build, or EndTurn.
// Appends the resulting events. Returns true if the side still has its
// turn afterwards (call again), false once it ended its turn or the
// battle is over. Deterministic: uses no randomness of its own.
//
// The player calls this once per animation beat so each AI move can be
// shown; headless tools just loop on it.
bool aiTakeStep(Battle& battle, std::vector<Event>& events);

}  // namespace ote
