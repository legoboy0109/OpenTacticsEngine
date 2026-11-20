#include "core/battle/save_faction.h"

namespace ote {
namespace savejson {

void writeSideFaction(const Database& db, const SideState& s, Json& side) {
  if (s.faction >= 0 && (size_t)s.faction < db.factions.size()) side.set("faction", Json::makeString(db.factions[(size_t)s.faction].id));
}

bool readSideFaction(Reader& rd, const Database& db, const Json& side, const std::string& where, SideState& s) {
  s.faction = -1;
  if (!side.has("faction")) return true;
  const std::string at = where + ".faction";
  if (db.factions.empty()) return rd.fail(at, "the game has no factions");
  if (!side["faction"].isString()) return rd.fail(at, "not a faction id");
  const std::string id = side["faction"].asString();
  const int f = db.findFaction(id);
  if (f < 0) return rd.fail(at, "names unknown faction \"" + id + "\" (data/factions.json)");
  s.faction = f;
  std::vector<SideModifier> mods;
  factionSideModifiers(db, f, mods);
  s.modifiers.insert(s.modifiers.begin(), mods.begin(), mods.end());
  return true;
}

}  // namespace savejson
}  // namespace ote
