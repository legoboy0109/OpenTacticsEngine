// The keys of a "behaviour" block that make a game's computer player behave like a scripted one (docs/AI.md "Scripted-style computer players"):
// order, weights, caution, retreat, idleTurns, route (mode patrol), ignores, measure (mode guardArea) and heals. All optional; a block without
// them reads, writes and plays exactly as before.
#include <cmath>

#include "core/data/behaviour.h"
#include "core/data/database.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }
bool whole(const Json& j) { return j.isNumber() && j.asNumber() == std::floor(j.asNumber()) && std::fabs(j.asNumber()) < 2e9; }
bool no(std::string& err, const std::string& msg) {
  err = msg;
  return false;
}

bool readRetreat(const Json& v, const Database& db, AiRetreat& r, std::string& err) {
  const std::string hint = "\"retreat\" must be an object such as { \"below\": 30, \"until\": 80, \"to\": { \"terrain\": [\"fort\"] }, \"useItems\": true }";
  if (!v.isObject()) return no(err, hint);
  for (const Json::Member& m : v.members())
    if (m.first != "below" && m.first != "until" && m.first != "to" && m.first != "useItems")
      return no(err, "\"retreat\" has an unknown key " + q(m.first) + " (it may have below, until, to, useItems)" + didYouMean(m.first, {"below", "until", "to", "useItems"}));
  if (!v.has("below")) return no(err, "\"retreat\" needs \"below\" (the hit point percent that sends the unit back)");
  if (!whole(v["below"]) || v["below"].asInt() < 1 || v["below"].asInt() > 99) return no(err, "\"retreat\" \"below\" must be a whole number from 1 to 99 (percent of its hit points)");
  r.below = v["below"].asInt();
  r.until = 100;
  if (v.has("until")) {
    if (!whole(v["until"]) || v["until"].asInt() < 2 || v["until"].asInt() > 100) return no(err, "\"retreat\" \"until\" must be a whole number from 2 to 100 (percent of its hit points)");
    r.until = v["until"].asInt();
  }
  if (r.until <= r.below) return no(err, "\"retreat\" \"until\" (" + std::to_string(r.until) + ") must be more than \"below\" (" + std::to_string(r.below) + ")");
  if (v.has("useItems")) {
    if (!v["useItems"].isBool()) return no(err, "\"retreat\" \"useItems\" must be true or false");
    r.useItems = v["useItems"].asBool();
  }
  if (v.has("to")) {
    const Json& to = v["to"];
    if (!to.isObject()) return no(err, "\"retreat\" \"to\" must be { \"terrain\": [\"fort\"] }");
    for (const Json::Member& m : to.members())
      if (m.first != "terrain") return no(err, "\"retreat\" \"to\" has an unknown key " + q(m.first) + " (it may have terrain)" + didYouMean(m.first, {"terrain"}));
    if (!to["terrain"].isArray() || to["terrain"].size() == 0) return no(err, "\"retreat\" \"to\" needs \"terrain\": a list of terrain ids, such as [\"fort\"]");
    for (const Json& t : to["terrain"].items()) {
      if (!t.isString()) return no(err, "\"retreat\" \"to\" \"terrain\" must be a list of terrain ids");
      const int idx = db.findTerrain(t.asString());
      if (idx < 0) return no(err, "\"retreat\" \"to\" names unknown terrain " + q(t.asString()));
      r.terrainIds.push_back(t.asString());
      r.terrains.push_back(idx);
    }
  }
  r.on = true;
  return true;
}

bool readRoute(const Json& v, AiBehaviour& b, std::string& err) {
  if (!v.isArray() || (int)v.size() < 2 || (int)v.size() > kMaxRouteSquares)
    return no(err, "\"route\" must be a list of 2 to " + std::to_string(kMaxRouteSquares) + " squares such as [ { \"x\": 3, \"y\": 4 }, { \"x\": 8, \"y\": 4 } ]");
  for (const Json& s : v.items()) {
    if (!s.isObject()) return no(err, "every square of \"route\" must look like { \"x\": 3, \"y\": 4 }");
    for (const Json::Member& m : s.members())
      if (m.first != "x" && m.first != "y") return no(err, "a square of \"route\" has an unknown key " + q(m.first) + " (it has x and y)" + didYouMean(m.first, {"x", "y"}));
    if (!whole(s["x"]) || !whole(s["y"]) || s["x"].asInt() < 0 || s["y"].asInt() < 0 || s["x"].asInt() > 9999 || s["y"].asInt() > 9999)
      return no(err, "every square of \"route\" needs a whole number \"x\" and \"y\" (0 or more)");
    b.route.push_back(std::make_pair(s["x"].asInt(), s["y"].asInt()));
  }
  for (size_t i = 0; i < b.route.size(); ++i)
    if (b.route[i] == b.route[(i + 1) % b.route.size()])
      return no(err, "\"route\" has the same square twice in a row (" + std::to_string(b.route[i].first) + "," + std::to_string(b.route[i].second) +
                         "; the last square is followed by the first)");
  return true;
}

}  // namespace

bool scriptedKey(const std::string& key) {
  static const char* const kKeys[] = {"order", "weights", "caution", "retreat", "idleTurns", "route", "ignores", "measure", "heals", "staffFirst", "staffPick", "inflictOnlyOn"};
  for (const char* k : kKeys)
    if (key == k) return true;
  return false;
}

bool scriptedIsDefault(const AiBehaviour& b) {
  return b.order == 0 && b.weightSet < 0 && !b.cautionNone && !b.retreat.on && b.idleTurns == 0 && b.route.empty() && b.ignores.empty() && !b.areaMoveCost && !b.healsRetreating &&
         b.staffFirst < 0 && !b.staffLastPlaced && !b.inflictCasters;
}

bool readScripted(const Json& v, const Database& db, AiBehaviour& b, std::string& err) {
  if (v.has("order")) {
    if (!whole(v["order"]) || std::fabs(v["order"].asNumber()) > kMaxBehaviourOrder)
      return no(err, "\"order\" must be a whole number from -" + std::to_string(kMaxBehaviourOrder) + " to " + std::to_string(kMaxBehaviourOrder));
    b.order = v["order"].asInt();
  }
  if (v.has("weights")) {
    if (!v["weights"].isString() || v["weights"].asString().empty()) return no(err, "\"weights\" must be the name of a set of the game's rules.ai.weights");
    b.weightsName = v["weights"].asString();
    b.weightSet = db.rules.ai.findWeights(b.weightsName);
    if (b.weightSet < 0)
      return no(err, "\"weights\" names " + q(b.weightsName) + ", which is not a set of the game's rules.ai.weights (add it in the game's rules, Computer player)");
  }
  if (v.has("caution")) {
    const std::string c = v["caution"].asString();
    if (!v["caution"].isString() || (c != "normal" && c != "none"))
      return no(err, "\"caution\" must be \"normal\" (the default) or \"none\" (it ignores danger when it walks toward the enemy)");
    b.cautionNone = c == "none";
  }
  if (v.has("retreat") && !readRetreat(v["retreat"], db, b.retreat, err)) return false;
  if (v.has("idleTurns")) {
    if (!whole(v["idleTurns"]) || v["idleTurns"].asInt() < 1 || v["idleTurns"].asInt() > kMaxBehaviourTurn)
      return no(err, "\"idleTurns\" must be a whole number from 1 to " + std::to_string(kMaxBehaviourTurn));
    b.idleTurns = v["idleTurns"].asInt();
  }
  if (v.has("route") && b.mode != AiMode::Patrol) return no(err, "\"route\" goes with mode \"patrol\"");
  if (b.mode == AiMode::Patrol) {
    if (!v.has("route")) return no(err, "mode \"patrol\" needs a \"route\": the squares it walks, such as [ { \"x\": 3, \"y\": 4 }, { \"x\": 8, \"y\": 4 } ]");
    if (!readRoute(v["route"], b, err)) return false;
  }
  if (v.has("ignores")) {
    if (!v["ignores"].isArray() || (int)v["ignores"].size() > kMaxIgnores)
      return no(err, "\"ignores\" must be a list of at most " + std::to_string(kMaxIgnores) + " unit ids (or character ids)");
    for (const Json& i : v["ignores"].items()) {
      if (!i.isString() || i.asString().empty()) return no(err, "\"ignores\" must be a list of unit ids (or character ids)");
      b.ignores.push_back(i.asString());
    }
  }
  if (v.has("measure")) {
    if (b.mode != AiMode::GuardArea || !v.has("within")) return no(err, "\"measure\" goes with mode \"guardArea\" and \"within\"");
    const std::string m = v["measure"].asString();
    if (!v["measure"].isString() || (m != "steps" && m != "moveCost"))
      return no(err, "\"measure\" must be \"steps\" (squares, the default) or \"moveCost\" (what the walk costs the unit; \"within\" is then a cost)");
    b.areaMoveCost = m == "moveCost";
  }
  if (v.has("heals")) {
    const std::string h = v["heals"].asString();
    if (!v["heals"].isString() || (h != "anyone" && h != "retreating"))
      return no(err, "\"heals\" must be \"anyone\" (the default) or \"retreating\" (its healing staves are used only on allies that are retreating to heal)");
    b.healsRetreating = h == "retreating";
  }
  if (v.has("staffFirst")) {
    if (!whole(v["staffFirst"]) || v["staffFirst"].asInt() < 0 || v["staffFirst"].asInt() > 100)
      return no(err, "\"staffFirst\" must be a whole number from 0 to 100 (the chance in percent that the unit uses a staff it could use now before its usual choice; the game rolls 0 to 99 and it acts when the roll is at most this)");
    b.staffFirst = v["staffFirst"].asInt();
  }
  if (v.has("staffPick")) {
    const std::string h = v["staffPick"].asString();
    if (!v["staffPick"].isString() || (h != "best" && h != "lastPlaced"))
      return no(err, "\"staffPick\" must be \"best\" (the default: the use the computer scores highest) or \"lastPlaced\" (its staves go to the unit placed last among the legal targets)");
    b.staffLastPlaced = h == "lastPlaced";
  }
  if (v.has("inflictOnlyOn")) {
    const std::string h = v["inflictOnlyOn"].asString();
    if (!v["inflictOnlyOn"].isString() || (h != "anyone" && h != "casters"))
      return no(err, "\"inflictOnlyOn\" must be \"anyone\" (the default) or \"casters\" (a staff whose condition stops staves is used only on a unit that carries a staff or a weapon that condition would stop)");
    b.inflictCasters = h == "casters";
  }
  return true;
}

void writeScripted(const AiBehaviour& b, Json& o) {
  if (b.order != 0) o.set("order", Json::makeNumber(b.order));
  if (b.weightSet >= 0) o.set("weights", Json::makeString(b.weightsName));
  if (b.cautionNone) o.set("caution", Json::makeString("none"));
  if (b.retreat.on) {
    Json r = Json::makeObject();
    r.set("below", Json::makeNumber(b.retreat.below));
    r.set("until", Json::makeNumber(b.retreat.until));
    if (!b.retreat.terrainIds.empty()) {
      Json to = Json::makeObject();
      Json list = Json::makeArray();
      for (const std::string& t : b.retreat.terrainIds) list.push(Json::makeString(t));
      to.set("terrain", list);
      r.set("to", to);
    }
    if (!b.retreat.useItems) r.set("useItems", Json::makeBool(false));
    o.set("retreat", r);
  }
  if (b.idleTurns > 0) o.set("idleTurns", Json::makeNumber(b.idleTurns));
  if (!b.route.empty()) {
    Json list = Json::makeArray();
    for (const std::pair<int, int>& s : b.route) {
      Json sq = Json::makeObject();
      sq.set("x", Json::makeNumber(s.first));
      sq.set("y", Json::makeNumber(s.second));
      list.push(sq);
    }
    o.set("route", list);
  }
  if (!b.ignores.empty()) {
    Json list = Json::makeArray();
    for (const std::string& i : b.ignores) list.push(Json::makeString(i));
    o.set("ignores", list);
  }
  if (b.areaMoveCost) o.set("measure", Json::makeString("moveCost"));
  if (b.healsRetreating) o.set("heals", Json::makeString("retreating"));
  if (b.staffFirst >= 0) o.set("staffFirst", Json::makeNumber(b.staffFirst));
  if (b.staffLastPlaced) o.set("staffPick", Json::makeString("lastPlaced"));
  if (b.inflictCasters) o.set("inflictOnlyOn", Json::makeString("casters"));
}

}  // namespace ote
