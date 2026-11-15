// Fog of war data (docs/ECONOMY.md "Fog of war"): units.json "vision", terrain.json / structures.json
// "visionBonus" and "hides", the map's "fog". Every key is optional; a game that uses none of them loads
// exactly as before.
#include "core/data/loader_content.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {

bool inRange(const LoadCtx& c, const Json& o, const char* key, const std::string& where, int lo, int hi, int& out) {
  if (!c.integer(o, key, where, out)) return false;
  if (o.has(key) && (out < lo || out > hi))
    return c.fail(where, std::string("\"") + key + "\" must be from " + std::to_string(lo) + " to " + std::to_string(hi));
  return true;
}

}  // namespace

bool readUnitVision(const LoadCtx& c, const Json& e, const std::string& where, UnitTypeDef& u) {
  return inRange(c, e, "vision", where, 0, kMaxVision, u.vision);
}

bool isUnitKey(const Database& db, const std::string& key) {
  if (db.findUnitType(key) >= 0) return true;
  for (const UnitTypeDef& u : db.unitTypes)
    for (const std::string& tag : u.tags)
      if (tag == key) return true;
  return false;
}

bool readSquareVision(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, int& visionBonus,
                      std::vector<std::string>& visionUnits, bool& hides) {
  if (!c.boolean(e, "hides", where, hides)) return false;
  if (!e.has("visionBonus") || !e["visionBonus"].isObject()) return inRange(c, e, "visionBonus", where, 0, kMaxVisionBonus, visionBonus);
  // The object form: the amount for some unit types only.
  const Json& o = e["visionBonus"];
  const std::string at = where + ".visionBonus";
  for (const Json::Member& m : o.members())
    if (m.first != "amount" && m.first != "units") return c.fail(at, "unknown key \"" + m.first + "\" (it has \"amount\" and \"units\")");
  if (!o.has("amount")) return c.fail(at, "needs \"amount\"");
  if (!o.has("units")) return c.fail(at, "needs \"units\" (leave the object out for a bonus that is for every unit)");
  if (!inRange(c, o, "amount", at, 0, kMaxVisionBonus, visionBonus)) return false;
  if (!c.stringList(o, "units", at, visionUnits)) return false;
  if (visionUnits.empty()) return c.fail(at, "\"units\" is empty (use a plain number for a bonus that is for every unit)");
  for (const std::string& key : visionUnits)
    if (!isUnitKey(db, key))
      return c.fail(at, "\"units\" entry \"" + key + "\" is neither a unit type id nor a tag used by any unit type");
  return true;
}

bool readMapFog(const LoadCtx& c, const Json& root, MapDef& m) {
  if (!c.boolean(root, "fog", "", m.fog)) return false;
  // The "seen by" mask of an event has one bit per team, and a team needs at least one side.
  if (m.fog && (int)m.sides.size() > kMaxFogTeams)
    return c.fail("fog", "fog of war supports at most " + std::to_string(kMaxFogTeams) + " sides (this map has " +
                             std::to_string(m.sides.size()) + ")");
  return true;
}

}  // namespace ote
