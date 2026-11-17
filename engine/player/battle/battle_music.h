// What the music director is told when a turn starts (docs/SOUND.md "Music by situation"): whose turn, whether a computer plays
// it, and how many units hostile to it are left. Presentation only; reads the Battle, never changes it.
#pragma once
#include "core/battle/battle.h"
#include "sound/music_director.h"

namespace ote {

// Units still on the board (alive) that belong to a side of another team than `side`'s. A human side's phase is the one the
// few-enemies music cares about; the count is made when the phase starts.
int hostileUnitsLeft(const Battle& b, int side);

PhaseInfo phaseInfo(const Battle& b, int side);

}  // namespace ote
