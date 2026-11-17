// Event loader, part 1: ids, unit filters, rectangles and conditions.
#include <algorithm>

#include "core/data/loader_content.h"
#include "core/data/loader_events.h"
#include "core/data/loader_difficulty.h"

namespace ote {

bool isStoryId(const std::string& id) {
  if (id.empty() || id.size() > 64) return false;
  for (char ch : id) {
    bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
    if (!ok) return false;
  }
  return true;
}

namespace {

std::string quoted(const std::string& s) { return "\"" + s + "\""; }

// Team names in the order Battle::init numbers them (first appearance among the sides).
std::vector<std::string> teamNames(const MapDef& map) {
  std::vector<std::string> names;
  for (const MapSideDef& s : map.sides) {
    std::string team = s.team.empty() ? s.id : s.team;
    if (std::find(names.begin(), names.end(), team) == names.end()) names.push_back(team);
  }
  return names;
}

bool parseOp(const std::string& t, CmpOp& op) {
  if (t == "=" || t == "==") op = CmpOp::Eq;
  else if (t == "!=") op = CmpOp::Ne;
  else if (t == "<") op = CmpOp::Lt;
  else if (t == "<=") op = CmpOp::Le;
  else if (t == ">") op = CmpOp::Gt;
  else if (t == ">=") op = CmpOp::Ge;
  else return false;
  return true;
}

}  // namespace

bool StoryLoader::onlyKeys(const Json& o, std::initializer_list<const char*> keys, const std::string& where) const {
  for (const Json::Member& m : o.members()) {
    bool known = false;
    for (const char* k : keys) known = known || m.first == k;
    if (known) continue;
    std::string list;
    for (const char* k : keys) list += (list.empty() ? "" : ", ") + quoted(k);
    return fail(where, "unknown key " + quoted(m.first) + " (expected " + list + ")");
  }
  return true;
}

bool StoryLoader::sideIndex(const std::string& id, const std::string& where, const char* what, int& out) const {
  out = findMapSide(map_, id);
  if (out >= 0) return true;
  return fail(where, std::string(what) + " names unknown side " + quoted(id));
}

bool StoryLoader::teamIndex(const std::string& id, const std::string& where, const char* what, int& out) const {
  std::vector<std::string> names = teamNames(map_);
  for (size_t i = 0; i < names.size(); ++i)
    if (names[i] == id) { out = (int)i; return true; }
  int side = findMapSide(map_, id);  // a side id means the side's team
  if (side >= 0) {
    std::string team = map_.sides[(size_t)side].team.empty() ? id : map_.sides[(size_t)side].team;
    out = (int)(std::find(names.begin(), names.end(), team) - names.begin());
    return true;
  }
  return fail(where, std::string(what) + " names unknown team " + quoted(id));
}

bool StoryLoader::unitId(const Json& j, const std::string& where, const char* what, std::string& out) const {
  if (!j.isString()) return fail(where, std::string(what) + " must be a unit id (text)");
  out = j.asString();
  if (std::find(unitIds.begin(), unitIds.end(), out) == unitIds.end())
    return fail(where, std::string(what) + " names unknown unit " + quoted(out) +
                           " (give the unit an \"id\" in the map's units)");
  return true;
}

bool StoryLoader::filter(const Json& j, const std::string& where, const char* what, UnitFilter& out) const {
  out = UnitFilter();
  if (j.isString()) {
    out.kind = UnitFilter::Kind::Unit;
    return unitId(j, where, what, out.unit);
  }
  std::string w = std::string(what);
  if (!j.isObject())
    return fail(where, w + " must be a unit filter: { \"unit\": id }, { \"side\": id }, { \"team\": name }, "
                           "{ \"type\": id }, { \"tag\": text } or {} for any unit");
  if (j.size() == 0) return true;
  if (j.size() > 1) return fail(where, w + " names more than one thing; a unit filter has one of unit, side, team, type, tag");
  const Json::Member& m = j.members()[0];
  const Json& v = m.second;
  if (m.first == "unit") {
    out.kind = UnitFilter::Kind::Unit;
    return unitId(v, where, (w + ".unit").c_str(), out.unit);
  }
  if (m.first == "tag") {
    if (!v.isString()) return fail(where, w + ".tag must be text");
    out.kind = UnitFilter::Kind::Tag;
    out.unit = v.asString();
    return true;
  }
  if (m.first != "side" && m.first != "team" && m.first != "type")
    return fail(where, w + ": unknown key " + quoted(m.first) + " (expected unit, side, team, type or tag)");
  if (!v.isString()) return fail(where, w + "." + m.first + " must be text");
  if (m.first == "side") {
    out.kind = UnitFilter::Kind::Side;
    return sideIndex(v.asString(), where, (w + ".side").c_str(), out.index);
  }
  if (m.first == "team") {
    out.kind = UnitFilter::Kind::Team;
    std::vector<std::string> names = teamNames(map_);
    auto it = std::find(names.begin(), names.end(), v.asString());
    if (it == names.end()) return fail(where, w + ".team names unknown team " + quoted(v.asString()));
    out.index = (int)(it - names.begin());
    return true;
  }
  out.kind = UnitFilter::Kind::Type;
  out.index = db_.findUnitType(v.asString());
  if (out.index < 0) return fail(where, w + ".type names unknown unit type " + quoted(v.asString()));
  return true;
}

bool StoryLoader::rect(const Json& j, const std::string& where, Rect& out) const {
  if (!j.isObject()) return fail(where, "\"reach\" must be an area: { \"x\": 5, \"y\": 2, \"w\": 2, \"h\": 1 }");
  if (!onlyKeys(j, {"x", "y", "w", "h"}, where)) return false;
  out = Rect();
  if (!c_.integer(j, "x", where, out.x, true) || !c_.integer(j, "y", where, out.y, true) ||
      !c_.integer(j, "w", where, out.w) || !c_.integer(j, "h", where, out.h))
    return false;
  if (out.w < 1 || out.h < 1) return fail(where, "\"w\" and \"h\" of an area must be at least 1");
  if (out.x < 0 || out.y < 0 || out.x + out.w > map_.width || out.y + out.h > map_.height)
    return fail(where, "the area " + std::to_string(out.x) + "," + std::to_string(out.y) + " " + std::to_string(out.w) +
                           " x " + std::to_string(out.h) + " does not fit inside the map (" + std::to_string(map_.width) +
                           " x " + std::to_string(map_.height) + ")");
  return true;
}

bool StoryLoader::condition(const Json& e, const std::string& where, Condition& out) const {
  if (!e.isObject()) return fail(where, "a condition must be an object");
  // "hasSkill" and "support" are the strategy-RPG conditions (docs/SRPG.md), read by their own areas below.
  static const char* const kKinds[] = {"switch", "variable", "alive", "dead", "round", "carries", "researched", "sees",
                                       "hasSkill", "support", "hasCondition", "actor", "difficulty", "rosterAlive", "rosterDead", "inRoster"};
  const char* kind = nullptr;
  for (const char* k : kKinds)
    if (e.has(k)) {
      if (kind) return fail(where, std::string("has two conditions (") + kind + " and " + k + "); use one entry for each");
      kind = k;
    }
  if (!kind) return fail(where, "is not a condition: it needs one of switch, variable, alive, dead, round, carries, researched, sees");
  std::string k = kind;
  const Json& v = e[kind];
  if (k == "hasSkill") {
    out.kind = Condition::Kind::HasSkill;
    return hasSkillCondition(e, where, out);
  }
  if (k == "difficulty") {  // docs/DIFFICULTY.md
    out.kind = Condition::Kind::Difficulty;
    return readDifficultyCondition(c_, db_, e, where, out.index, out.op);
  }
  if (k == "rosterAlive" || k == "rosterDead" || k == "inRoster") return rosterCondition(e, where, kind, out);  // docs/EVENTS.md
  if (k == "actor") {
    out.kind = Condition::Kind::Actor;
    return actorCondition(e, where, out);
  }
  if (k == "hasCondition") {
    out.kind = Condition::Kind::HasCondition;
    return hasConditionCondition(e, where, out);
  }
  if (k == "support") {
    out.kind = Condition::Kind::Support;
    return supportCondition(e, where, out);
  }
  if (k == "switch") {
    if (!onlyKeys(e, {"switch", "is"}, where)) return false;
    out.kind = Condition::Kind::Switch;
    out.is = true;
    if (!v.isString() || (out.index = db_.findSwitch(v.asString())) < 0)
      return fail(where, "\"switch\" names unknown switch " + quoted(v.asString()) + " (game.json \"switches\")");
    return c_.boolean(e, "is", where, out.is);
  }
  if (k == "variable") {
    if (!onlyKeys(e, {"variable", "op", "value"}, where)) return false;
    out.kind = Condition::Kind::Variable;
    if (!v.isString() || (out.index = db_.findVariable(v.asString())) < 0)
      return fail(where, "\"variable\" names unknown variable " + quoted(v.asString()) + " (game.json \"variables\")");
    std::string op = "=";
    if (!c_.str(e, "op", where, op) || !c_.integer(e, "value", where, out.value, true)) return false;
    if (!parseOp(op, out.op)) return fail(where, "\"op\" must be one of = != < <= > >=, not " + quoted(op));
    return true;
  }
  if (k == "round") {
    out.kind = Condition::Kind::Round;
    if (!v.isObject()) return fail(where, "\"round\" must be { \"op\": \">=\", \"value\": 5 }");
    if (!onlyKeys(e, {"round"}, where) || !onlyKeys(v, {"op", "value"}, where + ": \"round\"")) return false;
    std::string op = "=";
    if (!c_.str(v, "op", where, op) || !c_.integer(v, "value", where, out.value, true)) return false;
    if (!parseOp(op, out.op)) return fail(where, "\"op\" must be one of = != < <= > >=, not " + quoted(op));
    return true;
  }
  if (k == "carries") {
    if (!onlyKeys(e, {"carries", "unit"}, where)) return false;
    out.kind = Condition::Kind::Carries;
    if (!v.isString() || (out.index = db_.findItem(v.asString())) < 0)
      return fail(where, "\"carries\" names unknown item " + quoted(v.asString()));
    return !e.has("unit") || filter(e["unit"], where, "\"unit\"", out.filter);
  }
  if (k == "researched") {
    if (!onlyKeys(e, {"researched", "side"}, where)) return false;
    out.kind = Condition::Kind::Researched;
    if (!v.isString() || (out.index = db_.findTechnology(v.asString())) < 0)
      return fail(where, "\"researched\" names unknown technology " + quoted(v.asString()) +
                             (db_.research.empty() ? " (this game has no data/research.json)" : ""));
    std::string side;
    return c_.str(e, "side", where, side, true) && sideIndex(side, where, "\"side\"", out.side);
  }
  if (k == "sees") {
    if (!onlyKeys(e, {"sees", "team"}, where)) return false;
    out.kind = Condition::Kind::Sees;
    if (!filter(v, where, "\"sees\"", out.filter)) return false;
    std::string team;
    return c_.str(e, "team", where, team, true) && teamIndex(team, where, "\"team\"", out.team);
  }
  if (!onlyKeys(e, {kind}, where)) return false;
  out.kind = k == "alive" ? Condition::Kind::Alive : Condition::Kind::Dead;
  return filter(v, where, ("\"" + k + "\"").c_str(), out.filter);
}

bool StoryLoader::conditions(const Json& list, const std::string& where, std::vector<Condition>& out) const {
  if (!list.isArray()) return fail(where, "\"if\" must be a list of conditions");
  for (size_t i = 0; i < list.size(); ++i) {
    Condition cnd;
    if (!condition(list[i], where + ": condition " + std::to_string(i + 1), cnd)) return false;
    out.push_back(cnd);
  }
  return true;
}

}  // namespace ote
