// Action points in saves (docs/SAVES.md, docs/SRPG.md "Action points"): a unit's "apAttacks" (attacks made this turn). Written only in a game with
// action points and only when not 0. The movement points a unit walked this turn are its "moveSpent" (save_skills.cpp writes and reads that
// one in an action-point game too); the pool itself is a resource of the side and saved with the resources.
#include "core/battle/save_srpg.h"

namespace ote {
namespace savejson {

void writeUnitAp(const Battle& b, const Unit& u, Json& j) {
  if (b.db->rules.ap.on && u.apAttacks > 0) j.set("apAttacks", Json::makeNumber(u.apAttacks));
}

bool readUnitAp(Reader& rd, const Battle& b, const Json& j, const std::string& where, Unit& u) {
  u.apAttacks = 0;
  if (!j.has("apAttacks")) return true;
  if (!b.db->rules.ap.on) return rd.fail(where + ".apAttacks", "this game has no action points");
  return rd.integer(j, "apAttacks", where, 1, b.db->rules.ap.attacksPerTurn, u.apAttacks);
}

}  // namespace savejson
}  // namespace ote
