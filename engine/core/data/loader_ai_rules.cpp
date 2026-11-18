// Loader: game.json "rules.ai" (docs/AI.md "Scripted-style computer players"): turnOrder and the named weight sets.
// Read after the unit types (a set's `priority` table names them).
#include <cmath>

#include "core/data/loader_content.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {

struct IntKey {
  const char* key;
  int AiWeightSet::*field;
  int lo, hi;
};

const IntKey kKeys[] = {
    {"dmg", &AiWeightSet::dmg, 0, 100},
    {"left", &AiWeightSet::left, 0, 100},
    {"friends", &AiWeightSet::friends, 0, 100},
    {"class", &AiWeightSet::cls, 0, 100},
    {"turn", &AiWeightSet::turn, 0, 100},
    {"counter", &AiWeightSet::counter, 0, 100},
    {"danger", &AiWeightSet::danger, 0, 100},
    {"wounds", &AiWeightSet::wounds, 0, 100},
    {"killValue", &AiWeightSet::killValue, 0, 1000},
    {"dmgCap", &AiWeightSet::dmgCap, 0, 1000},
    {"leftBase", &AiWeightSet::leftBase, 0, 1000},
    {"friendsCap", &AiWeightSet::friendsCap, 0, 1000},
    {"counterCap", &AiWeightSet::counterCap, 0, 1000},
    {"noCounterBonus", &AiWeightSet::noCounterBonus, 0, 1000},
    {"dangerDiv", &AiWeightSet::dangerDiv, 1, 1000},
    {"dangerCap", &AiWeightSet::dangerCap, 0, 1000}};

bool whole(const Json& v, int lo, int hi) { return v.isNumber() && v.asNumber() == std::floor(v.asNumber()) && v.asNumber() >= lo && v.asNumber() <= hi; }

bool readWeightSet(const LoadCtx& c, const Database& db, const std::string& name, const Json& e, AiWeightSet& out) {
  const std::string where = "rules.ai.weights \"" + name + "\"";
  if (!e.isObject()) return c.fail(where, "must be an object such as { \"dmg\": 1, \"counter\": 2 }");
  out.name = name;
  for (const Json::Member& m : e.members()) {
    bool known = m.first == "attackAtZero" || m.first == "priority";
    for (const IntKey& k : kKeys) known = known || m.first == k.key;
    if (!known)
      return c.fail(where, "unknown key \"" + m.first +
                               "\" (weights: dmg, left, friends, class, turn, counter, danger, wounds; bases and caps: killValue, dmgCap, leftBase, "
                               "friendsCap, counterCap, noCounterBonus, dangerDiv, dangerCap; also attackAtZero and priority)");
  }
  for (const IntKey& k : kKeys) {
    if (!e.has(k.key)) continue;
    if (!whole(e[k.key], k.lo, k.hi))
      return c.fail(where, std::string("\"") + k.key + "\" must be a whole number from " + std::to_string(k.lo) + " to " + std::to_string(k.hi));
    out.*(k.field) = e[k.key].asInt();
  }
  if (!c.boolean(e, "attackAtZero", where, out.attackAtZero)) return false;
  if (e.has("priority")) {
    if (!e["priority"].isObject()) return c.fail(where, "\"priority\" must be an object of unit type id: number, such as { \"archer\": 5 }");
    for (const Json::Member& m : e["priority"].members()) {
      const int type = db.findUnitType(m.first);
      if (type < 0) return c.fail(where, "\"priority\" names unknown unit type \"" + m.first + "\"");
      if (!whole(m.second, -1000, 1000)) return c.fail(where, "\"priority\" of \"" + m.first + "\" must be a whole number from -1000 to 1000");
      out.priority.push_back(std::make_pair(type, m.second.asInt()));
    }
  }
  return true;
}

}  // namespace

bool loadAiRules(const std::string& gameFile, Database& db, std::string& err) {
  Json root;
  if (!loadObjectFile(gameFile, root, err)) return false;
  LoadCtx c(err, gameFile);
  const Json* rules;
  if (!c.object(root, "rules", "", rules)) return false;
  if (!rules->has("ai")) return true;
  const Json* blk;
  if (!c.object(*rules, "ai", "rules", blk)) return false;
  for (const Json::Member& m : blk->members())
    if (m.first != "turnOrder" && m.first != "weights")
      return c.fail("rules.ai", "unknown key \"" + m.first + "\" (expected \"turnOrder\" or \"weights\")");
  std::string order = "best";
  if (!c.str(*blk, "turnOrder", "rules.ai", order)) return false;
  if (order != "best" && order != "order")
    return c.fail("rules.ai", "\"turnOrder\" must be \"best\" (the computer does its best action first) or \"order\" (its units act one at a time, in the order of their \"order\" numbers)");
  db.rules.ai.orderMode = order == "order";
  if (blk->has("weights")) {
    if (!(*blk)["weights"].isObject()) return c.fail("rules.ai", "\"weights\" must be an object of named sets, such as { \"classic\": { \"dmg\": 1 } }");
    for (const Json::Member& m : (*blk)["weights"].members()) {
      if (m.first.empty()) return c.fail("rules.ai.weights", "a set needs a name");
      AiWeightSet set;
      if (!readWeightSet(c, db, m.first, m.second, set)) return false;
      db.rules.ai.weights.push_back(set);
    }
  }
  return true;
}

}  // namespace ote
