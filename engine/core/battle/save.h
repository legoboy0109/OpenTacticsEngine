// Saving and restoring a battle in progress (contract: docs/SAVES.md).
// These functions only convert between a Battle and JSON; reading and
// writing save FILES is the player's job (engine/player/saves.*).
#pragma once
#include <string>

#include "core/base/json.h"
#include "core/battle/battle.h"

namespace ote {

// Everything that changes during play, as JSON: round, active side, result,
// the random generator, every side's changing state, every unit ever created
// and every structure's owner and capture state. What the map fixes
// (terrain, heights, structure positions and kinds) is not stored.
void saveBattleState(const Battle& battle, Json& out);

// Puts a saved state onto `battle`, which must be freshly initialised from
// the same game and map (Battle::init, no start()). Everything read is
// checked (types, counts, ranges, cells, structures); on any problem returns
// false with a readable message in `err` and leaves `battle` untouched
// (the state is built in a copy and assigned only on success).
bool restoreBattleState(Battle& battle, const Json& saved, std::string& err);

// Hash for a save's "dataHash": 64-bit FNV-1a over the bytes of the files
// that define the rules of this battle (game.json, data/weapons.json,
// units.json, terrain.json and structures.json when present, the scripts
// listed in game.json, and the map file), each preceded by its name and
// length. graphics.json, art and fonts are not part of it. Written as 16
// lowercase hex digits. False (with err) if a file cannot be read.
bool computeDataHash(const Database& db, const std::string& mapId, std::string& out, std::string& err);

}  // namespace ote
