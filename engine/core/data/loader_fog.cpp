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

bool readSquareVision(const LoadCtx& c, const Json& e, const std::string& where, int& visionBonus, bool& hides) {
  return inRange(c, e, "visionBonus", where, 0, kMaxVisionBonus, visionBonus) && c.boolean(e, "hides", where, hides);
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
