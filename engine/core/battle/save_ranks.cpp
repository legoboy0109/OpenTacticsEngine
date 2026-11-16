// Weapon ranks in saves (docs/SRPG.md section 7, "Saves and hash"): a unit's and a roster entry's weapon experience as
// "ranks": { "<group>": n }. Written only in a game with ranks and only for the groups above 0; a key in a game
// without ranks is refused.
#include "core/battle/save_srpg.h"

namespace ote {
namespace savejson {

void writeRankXp(const Database& db, const std::vector<int>& xp, Json& j) {
  if (!db.ranks.on()) return;
  Json o = Json::makeObject();
  for (size_t g = 0; g < xp.size() && g < db.ranks.groups.size(); ++g)
    if (xp[g] > 0) o.set(db.ranks.groups[g], Json::makeNumber(xp[g]));
  if (o.size() > 0) j.set("ranks", o);
}

bool readRankXp(Reader& rd, const Database& db, const Json& j, const std::string& where, std::vector<int>& out) {
  out.clear();
  if (!db.ranks.on()) {
    if (j.has("ranks")) return rd.fail(where + ".ranks", "this game has no weapon ranks");
    return true;
  }
  out.assign(db.ranks.groups.size(), 0);
  if (!j.has("ranks")) return true;
  const Json* o;
  if (!rd.object(j, "ranks", where, o)) return false;
  for (const Json::Member& m : o->members()) {
    const std::string at = where + ".ranks." + m.first;
    const int g = db.ranks.findGroup(m.first);
    if (g < 0) return rd.fail(at, "not a weapon group of this game");
    const Json& v = m.second;
    if (!v.isNumber() || v.asNumber() != std::floor(v.asNumber()) || v.asNumber() < 0 || v.asNumber() > db.ranks.topXp())
      return rd.fail(at, "must be a whole number from 0 to " + std::to_string(db.ranks.topXp()));
    out[(size_t)g] = (int)v.asNumber();
  }
  return true;
}

}  // namespace savejson
}  // namespace ote
