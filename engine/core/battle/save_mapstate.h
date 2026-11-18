// The part of a battle save that records changes to the map and to the fog switches (docs/SAVES.md "Map changes"): squares whose terrain is
// not what the map placed, the named changes applied (with what they replaced, so they can be reverted), and fog / vision when a story
// command switched them. Written only when something differs, so a battle that never changed the map saves exactly as before.
#pragma once
#include "core/base/json.h"
#include "core/battle/battle.h"
#include "core/battle/save_reader.h"

namespace ote {
namespace savejson {

// Adds "mapState": { "terrain": [[square, terrain], ...], "tiles": [[square, tile], ...], "applied": [{ "change", "before", "tiles", "kinds" }], "fog": bool, "fogVision": n }; each
// member only when it differs from the start of the battle. Does nothing when nothing does.
void writeMapState(const Battle& b, Json& out);
// Reads it into `b` (a copy being restored, still as the map placed it). No key = nothing changed. Every number is checked with a path.
bool readMapState(Reader& rd, const Json& saved, Battle& b);

}  // namespace savejson
}  // namespace ote
