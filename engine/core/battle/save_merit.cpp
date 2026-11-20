// The rank counter of "rules.ranks" in saves (docs/SAVES.md "Unit ranks"): `"merit": n` on a unit of a battle save and on a roster entry, written in games
// with the block only (a game without it gets no key, and a save that has one is refused). The rank itself is never saved: it derives from the counter.
#include "core/battle/save_srpg.h"

namespace ote {
namespace savejson {

void writeMerit(const Database& db, int merit, Json& j) {
  if (db.rules.unitRanks.merit) j.set("merit", Json::makeNumber(merit));
}

bool readMerit(Reader& rd, const Database& db, const Json& j, const std::string& where, int& out) {
  out = 0;
  if (!db.rules.unitRanks.merit) return !j.has("merit") || rd.fail(where + ".merit", "this game has no rank counter (\"rules.ranks\" in game.json)");
  if (!j.has("merit")) return rd.fail(where + ".merit", "missing (this game keeps a rank counter, \"rules.ranks\")");
  return rd.integer(j, "merit", where, 0, db.rules.unitRanks.topAt(), out);
}

}  // namespace savejson
}  // namespace ote
