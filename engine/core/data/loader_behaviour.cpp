// Loader: a computer unit's "behaviour" (map unit and addUnit) and the story commands setBehaviour / wakeGroup (docs/AI.md).
#include <algorithm>

#include "core/data/loader_content.h"
#include "core/data/loader_events.h"

namespace ote {

namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

// A unit id the map (or one of its addUnit commands) knows, or a character.
bool knownUnit(const Database& db, const std::vector<std::string>& ids, const std::string& id) {
  return std::find(ids.begin(), ids.end(), id) != ids.end() || db.findCharacter(id) >= 0;
}

}  // namespace

// What needs the map: squares inside it, a place that exists. `where` names the unit or command.
bool checkBehaviourPlace(const LoadCtx& c, const MapDef& m, const std::string& where, const AiBehaviour& b) {
  if (b.hasArea && (b.ax >= m.width || b.ay >= m.height))
    return c.fail(where, "\"area\" starts at " + std::to_string(b.ax) + "," + std::to_string(b.ay) + ", outside the map (" +
                             std::to_string(m.width) + " x " + std::to_string(m.height) + ")");
  if (b.seekCell && (b.sx >= m.width || b.sy >= m.height))
    return c.fail(where, "the square " + std::to_string(b.sx) + "," + std::to_string(b.sy) + " to head for is outside the map (" +
                             std::to_string(m.width) + " x " + std::to_string(m.height) + ")");
  for (size_t i = 0; i < b.route.size(); ++i)
    if (b.route[i].first >= m.width || b.route[i].second >= m.height)
      return c.fail(where, "\"route\" square " + std::to_string(i + 1) + " (" + std::to_string(b.route[i].first) + "," + std::to_string(b.route[i].second) +
                               ") is outside the map (" + std::to_string(m.width) + " x " + std::to_string(m.height) + ")");
  if (b.mode == AiMode::Escape && m.exits.empty())
    return c.fail(where, "mode \"escape\" needs squares to leave by: give the map \"exits\": [ { \"x\": 0, \"y\": 4 } ]");
  if (!b.seekPlace.empty()) {
    bool found = false;
    for (const MapStructureDef& s : m.structures) found = found || s.id == b.seekPlace;
    if (!found) return c.fail(where, "\"place\" names " + q(b.seekPlace) + ", which is not a structure of this map (give the structure an \"id\")");
  }
  return true;
}

bool checkBehaviourUnits(const LoadCtx& c, const Database& db, const std::vector<std::string>& ids, const std::string& where,
                         const AiBehaviour& b) {
  if (!b.seekUnit.empty() && !knownUnit(db, ids, b.seekUnit))
    return c.fail(where, "\"unit\" names unknown unit " + q(b.seekUnit) + " (give the unit an \"id\" in the map's units)");
  if (b.prefer == AiPrefer::Unit && !knownUnit(db, ids, b.preferText))
    return c.fail(where, "\"prefer\" names unknown unit " + q(b.preferText) + " (give the unit an \"id\" in the map's units)");
  for (const std::string& id : b.ignores)
    if (!knownUnit(db, ids, id)) return c.fail(where, "\"ignores\" names unknown unit " + q(id) + " (give the unit an \"id\" in the map's units, or use a character id)");
  return true;
}

bool readUnitBehaviour(const LoadCtx& c, const Database& db, const MapDef& m, const Json& e, const std::string& where, MapUnitDef& u) {
  if (!e.has("behaviour")) return true;
  if (u.side >= 0 && u.side < (int)m.sides.size() && !m.sides[(size_t)u.side].ai)
    return c.fail(where, "\"behaviour\" tells the computer opponent what to do, but this unit is on a side a person plays (controller \"human\")");
  std::string err;
  if (!parseBehaviour(e["behaviour"], db, u.behaviour, err)) return c.fail(where, err);
  // A looter with nothing to go for would only ever escape: say so instead (read after "goesFor", docs/AI.md "Looting and escaping").
  if (u.behaviour.mode == AiMode::Loot && !(u.aiGoals & (kAiChests | kAiPlaces | kAiDoors | kAiSteal)))
    return c.fail(where, "mode \"loot\" needs \"goesFor\" with what it takes: \"chests\", \"villages\", \"doors\" and / or \"steal\"");
  return checkBehaviourPlace(c, m, where, u.behaviour);
}

// The seek / prefer unit references of every map unit; run once the units (and the ids of addUnit commands) are known.
bool checkMapBehaviours(const Database& db, const Json& root, const MapDef& m, const std::string& file, std::string& err) {
  bool any = false;
  for (const MapUnitDef& u : m.units) any = any || !u.behaviour.seekUnit.empty() || u.behaviour.prefer == AiPrefer::Unit || !u.behaviour.ignores.empty();
  if (!any) return true;
  std::vector<std::string> ids;
  for (const MapUnitDef& u : m.units)
    if (!u.id.empty()) ids.push_back(u.id);
  if (root["events"].isArray())
    for (const Json& e : root["events"].items()) collectAddedUnitIds(e["do"], ids);
  LoadCtx c(err, file);
  for (size_t i = 0; i < m.units.size(); ++i) {
    const MapUnitDef& u = m.units[i];
    const std::string where = "units[" + std::to_string(i) + "] (" + db.unitTypes[(size_t)u.type].id + " at " + std::to_string(u.x) + "," +
                              std::to_string(u.y) + ")";
    if (!checkBehaviourUnits(c, db, ids, where, u.behaviour)) return false;
  }
  return true;
}

// Group names written in addUnit units ("behaviour") and setBehaviour blocks of a command list, nested commands included.
void collectBehaviourGroups(const Json& commands, std::vector<std::string>& out) {
  for (const Json& c : commands.items()) {
    if (!c.isObject()) continue;
    if (c["addUnit"].isObject() && c["addUnit"]["behaviour"].isObject() && c["addUnit"]["behaviour"]["group"].isString())
      out.push_back(c["addUnit"]["behaviour"]["group"].asString());
    if (c["setBehaviour"].isObject() && c["setBehaviour"]["group"].isString()) out.push_back(c["setBehaviour"]["group"].asString());
    collectBehaviourGroups(c["then"], out);
    collectBehaviourGroups(c["else"], out);
    for (const Json& a : c["answers"].items()) collectBehaviourGroups(a["do"], out);
  }
}

bool StoryLoader::behaviourCommand(const Json& e, const std::string& at, Command& out) {
  if (e.has("wakeGroup")) {
    out.kind = Command::Kind::WakeGroup;
    if (!onlyKeys(e, {"wakeGroup"}, at)) return false;
    if (!e["wakeGroup"].isString() || e["wakeGroup"].asString().empty()) return fail(at, "\"wakeGroup\" must be a group name");
    out.unit = e["wakeGroup"].asString();
    if (std::find(groups.begin(), groups.end(), out.unit) == groups.end())
      return fail(at, "\"wakeGroup\" names " + q(out.unit) + ", but no unit of this map is in a group of that name (give units \"group\": " +
                          q(out.unit) + " in their \"behaviour\")");
    return true;
  }
  out.kind = Command::Kind::SetBehaviour;
  if (!onlyKeys(e, {"setBehaviour", "units"}, at)) return false;
  std::string err;
  if (!parseBehaviour(e["setBehaviour"], db_, out.behaviour, err)) return fail(at, err);
  if (!checkBehaviourPlace(c_, map_, at, out.behaviour) || !checkBehaviourUnits(c_, db_, unitIds, at, out.behaviour)) return false;
  if (!e.has("units")) return fail(at, "needs \"units\": which units get the behaviour (a unit filter, {} for every unit)");
  if (!filter(e["units"], at, "\"units\"", out.filter)) return false;
  if (out.filter.kind == UnitFilter::Kind::Side && out.filter.index >= 0 && out.filter.index < (int)map_.sides.size() &&
      !map_.sides[(size_t)out.filter.index].ai)
    return fail(at, "\"units\" names a side a person plays; only the computer's units have a behaviour");
  return true;
}

}  // namespace ote
