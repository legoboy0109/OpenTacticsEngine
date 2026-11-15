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

}  // namespace savejson
}  // namespace ote
