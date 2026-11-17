// Battle save: the state of doors, chests and visitable places (docs/SAVES.md "Doors, chests and visiting"). Written only when
// the structure is one of those, so a save of any other game is unchanged.
#pragma once
#include "core/base/json.h"
#include "core/battle/battle.h"
#include "core/battle/save_reader.h"

namespace ote {
namespace savejson {

// "locked" (a door / chest kind), "hp" (a door that can be attacked), "used" (a place that was visited), "loot" (a chest's contents).
void writeStructurePlace(const Battle& b, const StructureState& s, Json& out);
// Restores them onto `st` (its kind is already restored). A key that does not fit the structure's present kind is refused.
bool readStructurePlace(Reader& rd, const Battle& b, const Json& j, const std::string& where, StructureState& st);

// A unit's "goes" (what the computer opponent goes for): written when not 0.
void writeUnitGoals(const Unit& u, Json& out);
bool readUnitGoals(Reader& rd, const Json& j, const std::string& where, Unit& u);

}  // namespace savejson
}  // namespace ote
