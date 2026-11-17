// The fog of war part of a battle save (docs/SAVES.md): what each team last saw of every structure's owner. Sight
// itself is not saved, it is recomputed from the units. Written only for maps with fog, so games without fog save
// exactly as before.
#pragma once
#include "core/base/json.h"
#include "core/battle/battle.h"
#include "core/battle/save_reader.h"

namespace ote {
namespace savejson {

// Adds "seenOwners": one list per team (in team order), each with one side index (-1 = nobody) per structure.
// Does nothing when the battle has no fog.
void writeSeenOwners(const Battle& b, Json& out);
// Reads it into `b` (a copy being restored). A fog battle must have the key; one without fog must not.
bool readSeenOwners(Reader& rd, const Json& saved, Battle& b);

// Adds "seenKinds" (same shape as seenOwners, one structure kind index per entry) ONLY when the battle has fog and
// some team last saw a structure as a kind other than the map's; so every save of a game that never upgraded
// anything out of sight is byte for byte what it was before.
void writeSeenKinds(const Battle& b, Json& out);
// Reads it into `b`. A missing key means every team believes the map's kinds. Refused on a battle without fog,
// and for an entry that is not a kind the structure can have become (same footprint, reachable by upgrades).
bool readSeenKinds(Reader& rd, const Json& saved, Battle& b);

// Adds "lights" (docs/SRPG.md section 8, stage 2): the lights staves made that still last, as [{ "side", "cell", "radius", "turns" }].
// Written only when there are some, so every other save is what it was.
void writeLights(const Battle& b, Json& out);
// Reads it into `b`. No key = no lights (an old save). Refused on a battle without fog and for an entry that is not a side, a square
// on the map, a radius 1..10 and turns 1..10.
bool readLights(Reader& rd, const Json& saved, Battle& b);

}  // namespace savejson
}  // namespace ote
