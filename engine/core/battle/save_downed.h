// Battle save: a downed unit (docs/SRPG.md "Downed units and revive", docs/SAVES.md). Written only for a downed unit, so every save of a game
// without rules.downed (and of a downed game with nobody downed) is exactly what it was.
#pragma once
#include "core/base/json.h"
#include "core/battle/battle.h"
#include "core/battle/save_reader.h"

namespace ote {
namespace savejson {

// "downed": true and, when not 0, "downedTurns": n.
void writeUnitDowned(const Unit& u, Json& out);
// Read right after "alive" and "cell": a downed unit is not alive, keeps its cell, is not hidden. Its hit points (0) are checked by the caller.
bool readUnitDowned(Reader& rd, const Battle& b, const Json& j, const std::string& where, Unit& u);

}  // namespace savejson
}  // namespace ote
