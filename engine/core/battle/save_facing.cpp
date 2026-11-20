// Facing in saves (docs/SAVES.md, docs/SRPG.md "Facing"): a unit's "facing", the number of the direction it looks in (Grid::neighbor numbering).
// Written for every unit that has one, only in a game with rules.facing; a game without it saves exactly what it always did. A save that lacks the
// key (a game that gained facing later) gives the unit the default facing when the battle is restored.
#include "core/battle/save_srpg.h"

namespace ote {
namespace savejson {

void writeUnitFacing(const Battle& b, const Unit& u, Json& j) {
  if (b.db->rules.facing.on && u.facing >= 0) j.set("facing", Json::makeNumber(u.facing));
}

bool readUnitFacing(Reader& rd, const Battle& b, const Json& j, const std::string& where, Unit& u) {
  u.facing = -1;
  if (!j.has("facing")) return true;
  if (!b.db->rules.facing.on) return rd.fail(where + ".facing", "this game has no facing");
  return rd.integer(j, "facing", where, 0, b.grid.directionCount() - 1, u.facing);
}

}  // namespace savejson
}  // namespace ote
