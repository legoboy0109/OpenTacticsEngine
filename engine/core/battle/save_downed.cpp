// Battle save: a downed unit (docs/SRPG.md "Downed units and revive", docs/SAVES.md).
#include "core/battle/save_downed.h"

namespace ote {
namespace savejson {

void writeUnitDowned(const Unit& u, Json& out) {
  if (!u.downed) return;
  out.set("downed", Json::makeBool(true));
  if (u.downedTurns > 0) out.set("downedTurns", Json::makeNumber(u.downedTurns));
}

bool readUnitDowned(Reader& rd, const Battle& b, const Json& j, const std::string& where, Unit& u) {
  u.downed = false;
  u.downedTurns = 0;
  if (!j.has("downed")) {
    if (j.has("downedTurns")) return rd.fail(where + ".downedTurns", "only a downed unit counts its turns downed");
    return true;
  }
  if (!rd.boolean(j, "downed", where, u.downed)) return false;
  if (!u.downed) return rd.fail(where + ".downed", "written only for a downed unit (true)");
  if (!b.db->rules.downed.on) return rd.fail(where + ".downed", "the game has no downed units (rules.downed)");
  if (u.alive) return rd.fail(where + ".downed", "a downed unit is not alive");
  if (u.cell == kNoCell) return rd.fail(where + ".cell", "a downed unit keeps its square");
  if (j.has("downedTurns") && !rd.integer(j, "downedTurns", where, 0, 99, u.downedTurns)) return false;
  return true;
}

}  // namespace savejson
}  // namespace ote
