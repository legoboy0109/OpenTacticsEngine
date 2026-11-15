// The fuel and ammunition part of a unit's save (docs/SAVES.md). Both keys are optional: they are written only
// for units whose type has fuel / weapons with ammunition, so games without the feature save exactly as before.
#pragma once
#include "core/base/json.h"
#include "core/battle/battle.h"
#include "core/battle/save_reader.h"

namespace ote {
namespace savejson {

// Adds "fuel": n and "ammo": { "<weapon id>": n, ... } to the unit's object where the unit has them.
void writeUnitSupply(const Database& db, const Unit& u, Json& out);
// Reads them into `u` (whose type is known). A unit that has fuel / ammunition must have the key; one that has
// none must not. Counts are 0 up to the full tank / load.
bool readUnitSupply(Reader& rd, const Database& db, const Json& j, const std::string& where, Unit& u);

}  // namespace savejson
}  // namespace ote
