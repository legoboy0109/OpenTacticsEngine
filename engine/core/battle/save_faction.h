// A side's faction in a battle save (docs/SAVES.md "Factions"): the key "faction": "<id>", written only when the side has one, so
// the saves of a game without factions stay byte for byte what they were. The side-wide skills a faction gives are not saved:
// they are rebuilt from the faction when the save is read (see factionSideModifiers).
#pragma once
#include "core/base/json.h"
#include "core/battle/battle.h"
#include "core/battle/save_reader.h"

namespace ote {
namespace savejson {

// Sets "faction" on `side` when `s` has one.
void writeSideFaction(const Database& db, const SideState& s, Json& side);
// Reads it into `s` (after readSideModifiers: the faction's skills go in FRONT of the saved ones, as they do in a live battle). No key = no
// faction. A faction that data/factions.json does not have is refused.
bool readSideFaction(Reader& rd, const Database& db, const Json& side, const std::string& where, SideState& s);

}  // namespace savejson
}  // namespace ote
