// Battle save: a computer unit's behaviour (docs/AI.md, docs/SAVES.md). Written only when it is not the default, so every save of a
// map that uses none of it is exactly what it was.
#pragma once
#include "core/base/json.h"
#include "core/battle/battle.h"
#include "core/battle/save_reader.h"

namespace ote {
namespace savejson {

// "behaviour" (the block as the map writes it) and "awake" (true once a guard has woken).
void writeUnitBehaviour(const Unit& u, Json& out);
bool readUnitBehaviour(Reader& rd, const Database& db, const Json& j, const std::string& where, Unit& u);

}  // namespace savejson
}  // namespace ote
