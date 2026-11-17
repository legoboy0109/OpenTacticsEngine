// Loader pieces of docs/EVENTS.md "Campaign characters in map events" and "Taking a unit off the map and bringing it back":
//   { "rosterAlive": "kay" } / { "rosterDead": "kay" } / { "inRoster": "kay", "party": "active" }   (map events and campaign scenes)
//   { "showUnit": "kay", "x": 4, "y": 2 }                                                            (map events)
// and the "hide" key of removeUnit, read in loader_story_commands.cpp.
#include "core/data/loader_events.h"
#include "core/data/loader_util.h"

namespace ote {

bool readRosterCondition(const LoadCtx& c, const Json& e, const std::string& where, const std::string& kind, int& mode, bool& anyParty,
                         std::string& id) {
  for (const Json::Member& m : e.members())
    if (m.first != kind && m.first != "party")
      return c.fail(where, "unknown key \"" + m.first + "\" (expected \"" + kind + "\" and optionally \"party\")");
  const Json& v = e[kind.c_str()];
  if (!v.isString() || !isStoryId(v.asString()))
    return c.fail(where, "\"" + kind + "\" must be a character id (letters, digits, \"_\" and \"-\")");
  id = v.asString();
  mode = kind == "rosterAlive" ? 0 : kind == "rosterDead" ? 1 : 2;
  anyParty = true;
  if (e.has("party")) {
    std::string party;
    if (!c.str(e, "party", where, party)) return false;
    if (party != "any" && party != "active") return c.fail(where, "\"party\" must be \"any\" or \"active\", not \"" + party + "\"");
    anyParty = party == "any";
  }
  return true;
}

bool StoryLoader::rosterCondition(const Json& e, const std::string& at, const char* kind, Condition& out) const {
  out.kind = Condition::Kind::Roster;
  out.filter = UnitFilter();
  out.filter.kind = UnitFilter::Kind::Unit;
  return readRosterCondition(c_, e, at, kind, out.rosterMode, out.anyParty, out.filter.unit);
}

bool StoryLoader::showUnitCommand(const Json& e, const std::string& at, Command& out) {
  out.kind = Command::Kind::ShowUnit;
  if (!onlyKeys(e, {"showUnit", "x", "y"}, at)) return false;
  // The unit is named by its id; it must be one this map knows (a unit placed by the map or an addUnit), as for moveUnit.
  if (!unitId(e["showUnit"], at, "\"showUnit\"", out.unit)) return false;
  if (!c_.integer(e, "x", at, out.x, true) || !c_.integer(e, "y", at, out.y, true)) return false;
  if (out.x < 0 || out.y < 0 || out.x >= map_.width || out.y >= map_.height)
    return fail(at, "the square " + std::to_string(out.x) + "," + std::to_string(out.y) + " is outside the map");
  return true;
}

}  // namespace ote
