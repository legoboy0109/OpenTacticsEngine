// Fog of war data (docs/ECONOMY.md "Fog of war"): units.json "vision", terrain.json / structures.json
// "visionBonus" and "hides", the map's "fog". Every key is optional; a game that uses none of them loads
// exactly as before.
#include "core/data/loader_content.h"
#include "core/data/loader_util.h"

namespace ote {

bool readUnitVision(const LoadCtx& c, const Json& e, const std::string& where, UnitTypeDef& u) {
  return c.integerFrom(e, "vision", where, 0, kMaxVision, u.vision);
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
  if (!e.has("visionBonus") || !e["visionBonus"].isObject()) return c.integerFrom(e, "visionBonus", where, 0, kMaxVisionBonus, visionBonus);
  // The object form: the amount for some unit types only.
  const Json& o = e["visionBonus"];
  const std::string at = where + ".visionBonus";
  for (const Json::Member& m : o.members())
    if (m.first != "amount" && m.first != "units") return c.fail(at, "unknown key \"" + m.first + "\" (it has \"amount\" and \"units\")");
  if (!o.has("amount")) return c.fail(at, "needs \"amount\"");
  if (!o.has("units")) return c.fail(at, "needs \"units\" (leave the object out for a bonus that is for every unit)");
  if (!c.integerFrom(o, "amount", at, 0, kMaxVisionBonus, visionBonus)) return false;
  if (!c.stringList(o, "units", at, visionUnits)) return false;
  if (visionUnits.empty()) return c.fail(at, "\"units\" is empty (use a plain number for a bonus that is for every unit)");
  for (const std::string& key : visionUnits)
    if (!isUnitKey(db, key))
      return c.fail(at, "\"units\" entry \"" + key + "\" is neither a unit type id nor a tag used by any unit type");
  return true;
}

bool readStructureLight(const LoadCtx& c, const Json& e, const std::string& where, StructureDef& s) {
  return c.integerFrom(e, "light", where, 1, kMaxStaffRadius, s.light);
}

bool readItemLight(const LoadCtx& c, const Json& e, const std::string& where, ItemDef& it) {
  if (!e.has("light")) return true;
  const Json& l = e["light"];
  const std::string at = where + ": \"light\"";
  if (!l.isObject()) return c.fail(where, "\"light\" must be { \"radius\": 3, \"turns\": 3 } (radius 1 to " + std::to_string(kMaxStaffRadius) + ", turns 1 to " + std::to_string(kMaxStaffLightTurns) + ")");
  for (const Json::Member& m : l.members())
    if (m.first != "radius" && m.first != "turns" && m.first != "shrink")
      return c.fail(at, "unknown key \"" + m.first + "\" (it has \"radius\", \"turns\" and \"shrink\")");
  if (!l.has("radius")) return c.fail(at, "needs \"radius\"");
  if (!c.integerFrom(l, "radius", at, 1, kMaxStaffRadius, it.lightRadius) || !c.integerFrom(l, "turns", at, 1, kMaxStaffLightTurns, it.lightTurns) ||
      !c.integerFrom(l, "shrink", at, 1, kMaxStaffRadius, it.lightShrink))
    return false;
  if (l.has("shrink") && it.lightShrink >= it.lightRadius) return c.fail(at, "\"shrink\" must be less than the radius");
  if (l.has("shrink") && !l.has("turns")) it.lightTurns = kMaxStaffLightTurns;  // burns down by itself (like a staff's light)
  return true;
}

bool readMapFog(const LoadCtx& c, const Json& root, MapDef& m) {
  if (!c.boolean(root, "fog", "", m.fog)) return false;
  if (!c.integerFrom(root, "fogVision", "", -kMaxFogVision, kMaxFogVision, m.fogVision)) return false;
  if (!c.boolean(root, "aiSeesAll", "", m.aiSeesAll)) return false;
  for (size_t i = 0; i < m.sides.size(); ++i)
    if (m.sides[i].aiSeesAll >= 0 && !m.sides[i].ai)
      return c.fail("side \"" + m.sides[i].id + "\"", "\"aiSeesAll\" is for computer sides (\"controller\": \"ai\")");
  if ((m.fogVision != 0 || m.aiSeesAll) && !m.fog) return c.fail("", "\"fogVision\" and \"aiSeesAll\" belong to a map with \"fog\": true");
  // The "seen by" mask of an event has one bit per team, and a team needs at least one side.
  if (m.fog && (int)m.sides.size() > kMaxFogTeams)
    return c.fail("fog", "fog of war supports at most " + std::to_string(kMaxFogTeams) + " sides (this map has " +
                             std::to_string(m.sides.size()) + ")");
  return true;
}

}  // namespace ote
