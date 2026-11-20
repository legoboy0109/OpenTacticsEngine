// Loader of the action-point system and of resource refills. Every refusal names the file, the entry and the key.
#include "core/data/loader_action_points.h"

namespace ote {

namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

// Refuses a key the block does not know, listing the ones it takes.
bool onlyThese(const LoadCtx& c, const Json& block, const std::string& at, std::initializer_list<const char*> keys) {
  for (const Json::Member& m : block.members()) {
    bool known = false;
    for (const char* k : keys) known = known || m.first == k;
    if (known) continue;
    std::string list;
    for (const char* k : keys) list += (list.empty() ? "" : ", ") + q(k);
    return c.fail(at, "unknown key " + q(m.first) + " (expected " + list + ")");
  }
  return true;
}

}  // namespace

bool loadResourceRefills(const LoadCtx& c, const Json& root, Database& db) {
  if (!root.has("resources")) return true;
  const Json& list = root["resources"];  // (readIdList already checked the shape)
  for (size_t i = 0; i < list.size() && i < db.resources.size(); ++i) {
    const Json& e = list[i];
    if (!e.has("refill")) continue;
    ResourceDef& r = db.resources[i];
    const std::string at = "resource " + q(r.id);
    const Json* blk;
    if (!c.object(e, "refill", at, blk, true)) return false;
    if (!onlyThese(c, *blk, at + ", \"refill\"", {"perTurn", "carryOver"})) return false;
    if (!blk->has("perTurn")) return c.fail(at, "\"refill\" needs \"perTurn\" (the amount the resource is brought up to at the start of the side's turn)");
    const std::string where = at + ", \"refill\"";
    if (!c.integerIn(*blk, "perTurn", where, 0, 100000, r.refillPerTurn)) return false;
    if (!c.integerIn(*blk, "carryOver", where, 0, 100000, r.refillCarryOver)) return false;
    r.refill = true;
  }
  return true;
}

bool loadActionPointRules(const LoadCtx& c, const Json& rules, Database& db) {
  ActionPointRules& ap = db.rules.ap;
  ap = ActionPointRules();
  if (!rules.has("actionPoints")) return true;
  const Json* blk;
  if (!c.object(rules, "actionPoints", "rules", blk, true)) return false;
  const std::string at = "rules.actionPoints";
  if (!onlyThese(c, *blk, at, {"resource", "move", "attack"})) return false;
  std::string id;
  if (!c.str(*blk, "resource", at, id, true)) return false;
  ap.resource = db.findResource(id);
  if (ap.resource < 0) return c.fail(at, "\"resource\" names unknown resource " + q(id) + " (list it under \"resources\" in game.json)");
  if (blk->has("move")) {
    const Json* m;
    if (!c.object(*blk, "move", at, m, true)) return false;
    const std::string w = at + ", \"move\"";
    if (!onlyThese(c, *m, w, {"costPerStep", "splitMoves", "softCap", "softCapPenaltyPercent"})) return false;
    if (!c.integerIn(*m, "costPerStep", w, 0, 99, ap.stepCost) || !c.boolean(*m, "splitMoves", w, ap.splitMoves)) return false;
    if (m->has("softCap") && !c.integerIn(*m, "softCap", w, 0, 999, ap.softCap)) return false;
    if (!c.integerIn(*m, "softCapPenaltyPercent", w, 0, 1000, ap.softCapPercent)) return false;
    if (m->has("softCapPenaltyPercent") && !m->has("softCap")) return c.fail(w, "\"softCapPenaltyPercent\" needs \"softCap\" (the number of movement points that cost the plain price)");
  }
  if (blk->has("attack")) {
    const Json* a;
    if (!c.object(*blk, "attack", at, a, true)) return false;
    const std::string w = at + ", \"attack\"";
    if (!onlyThese(c, *a, w, {"cost", "attacksPerTurn"})) return false;
    if (!c.integerIn(*a, "cost", w, 0, 99, ap.attackCost) || !c.integerIn(*a, "attacksPerTurn", w, 1, 99, ap.attacksPerTurn)) return false;
  }
  ap.on = true;
  return true;
}

bool readWeaponAp(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, WeaponDef& w) {
  w.apCost = -1;
  w.area.apExtra = 0;
  if (e.has("apCost")) {
    if (!db.rules.ap.on) return c.fail(where, "\"apCost\" needs \"rules\": { \"actionPoints\": ... } in game.json");
    if (!c.integerIn(e, "apCost", where, 0, 99, w.apCost)) return false;
  }
  if (e.has("area") && e["area"].isObject() && e["area"].has("apExtra")) {
    if (!db.rules.ap.on) return c.fail(where, "\"area\".\"apExtra\" needs \"rules\": { \"actionPoints\": ... } in game.json");
    if (!c.integerIn(e["area"], "apExtra", where + ", \"area\"", 0, 99, w.area.apExtra)) return false;
  }
  return true;
}

bool readUnitAp(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, UnitTypeDef& u) {
  u.apStep = -1;
  u.apSoftCap = -1;
  if (!e.has("ap")) return true;
  if (!db.rules.ap.on) return c.fail(where, "\"ap\" needs \"rules\": { \"actionPoints\": ... } in game.json");
  const Json* blk;
  if (!c.object(e, "ap", where, blk, true)) return false;
  const std::string at = where + ", \"ap\"";
  if (!onlyThese(c, *blk, at, {"step", "softCap"})) return false;
  return c.integerIn(*blk, "step", at, 0, 99, u.apStep) && c.integerIn(*blk, "softCap", at, 0, 999, u.apSoftCap);
}

}  // namespace ote
