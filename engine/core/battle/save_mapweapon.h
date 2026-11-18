// Battle save: the uses left of a structure's map weapon (docs/SAVES.md "Map weapons"). Written only when they differ from what the
// kind (or the placement) starts with, so a save of any other game is unchanged.
#pragma once
#include "core/base/json.h"
#include "core/battle/battle.h"
#include "core/battle/save_reader.h"

namespace ote {
namespace savejson {

// "weaponUses": the uses left (0 = spent) of structure `index`, when it differs from its kind's number.
void writeStructureWeapon(const Battle& b, int index, Json& out);
// Restores it onto `st` (its kind is already restored). A key on a structure whose kind has no weapon is refused.
bool readStructureWeapon(Reader& rd, const Battle& b, const Json& j, const std::string& where, StructureState& st);

}  // namespace savejson
}  // namespace ote
