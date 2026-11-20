// A side's commander in a battle save (docs/SAVES.md "Commanders"): the key
//   "commander": { "id": "<id>", "meter": n, "power": index, "left": turns, "uses": [n per power] }
// written only when the side has one ("power" and "left" only while a power is active), so the saves of a game without commanders stay byte for
// byte what they were. The side-wide skills (passive, and the active power's) are not saved: they are rebuilt from these when the save is read.
#pragma once
#include "core/base/json.h"
#include "core/battle/battle.h"
#include "core/battle/save_reader.h"

namespace ote {
namespace savejson {

// Sets "commander" on `side` when `s` has one.
void writeSideCommander(const Database& db, const SideState& s, Json& side);
// Reads it into `s` (after readSideModifiers and readSideFaction: the passive skills go behind the faction's and in front of the technologies', an
// active power's last, as in a live battle). No key = no commander, which the save of a side that has one (as `s` comes in, fresh from the map) is
// refused for. Everything is checked: the id, the meter's range, the active power and its turns, the use counts.
bool readSideCommander(Reader& rd, const Database& db, const Json& side, const std::string& where, SideState& s);

}  // namespace savejson
}  // namespace ote
