// Loader of the speed turn order and of charged actions. Every refusal names the file, the entry and the key.
#include "core/data/loader_speed.h"

#include <algorithm>

namespace ote {

namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

}  // namespace

bool loadSpeedRules(const LoadCtx& c, const Json& rules, Database& db) {
  SpeedRules& sp = db.rules.speed;
  sp = SpeedRules();
  db.rules.turnOrder = TurnOrder::Sides;
  if (!rules.has("turnOrder")) return true;
  const Json& v = rules["turnOrder"];
  const Json* blk = nullptr;
  std::string mode;
  if (v.isString()) mode = v.asString();
  else if (v.isObject()) {
    blk = &v;
    if (!c.str(v, "mode", "rules.turnOrder", mode, true)) return false;
  } else {
    return c.fail("rules", "\"turnOrder\" must be \"sides\", \"speed\" or { \"mode\": \"speed\", \"stat\": \"speed\" }");
  }
  if (mode != "sides" && mode != "speed")
    return c.fail("rules", "turnOrder " + q(mode) + " must be \"sides\" (each side moves all its units in turn) or \"speed\" (units act one at a time, by speed)");
  if (mode == "sides") {
    if (blk && blk->size() > 1) return c.fail("rules.turnOrder", "the other keys belong to \"mode\": \"speed\"");
    return true;
  }

  const std::string at = "rules.turnOrder";
  if (blk)
    for (const Json::Member& m : blk->members()) {
      static const char* const keys[] = {"mode", "stat", "threshold", "preview", "roundTicks", "ctCost"};
      bool known = false;
      for (const char* k : keys) known = known || m.first == k;
      if (!known)
        return c.fail(at, unknownKeyText(m.first, {"mode", "stat", "threshold", "preview", "roundTicks", "ctCost"}));
    }
  // The stat: named, or the stat called "speed" when the game has one.
  std::string statId = "speed";
  if (blk && !c.str(*blk, "stat", at, statId)) return false;
  sp.stat = db.findStat(statId);
  if (sp.stat < 0)
    return c.fail(at, blk && blk->has("stat") ? "\"stat\" names unknown stat " + q(statId) + " (list it under \"stats\" in game.json)"
                                              : "a speed game needs a stat for the clock: add a stat \"speed\" or name one with { \"mode\": \"speed\", \"stat\": \"<stat id>\" }");
  if (blk) {
    if (!c.integerIn(*blk, "threshold", at, 1, 100000, sp.threshold) || !c.integerIn(*blk, "preview", at, 1, 30, sp.preview)) return false;
    sp.roundTicks = std::max(1, sp.threshold / 10);
    if (!c.integerIn(*blk, "roundTicks", at, 1, 100000, sp.roundTicks)) return false;
    if (blk->has("ctCost")) {
      const Json* cost;
      if (!c.object(*blk, "ctCost", at, cost, true)) return false;
      const std::string w = at + ", \"ctCost\"";
      for (const Json::Member& m : cost->members())
        if (m.first != "both" && m.first != "one" && m.first != "none")
          return c.fail(w, unknownKeyText(m.first, {"both", "one", "none"}));
      if (!c.integerIn(*cost, "both", w, 0, 100000, sp.costBoth) || !c.integerIn(*cost, "one", w, 0, 100000, sp.costOne) ||
          !c.integerIn(*cost, "none", w, 0, 100000, sp.costNone))
        return false;
    }
  } else {
    sp.roundTicks = std::max(1, sp.threshold / 10);
  }
  // A turn must cost something, or a unit would act again at once, forever.
  if (sp.costBoth < 1 || sp.costOne < 1 || sp.costNone < 1)
    return c.fail(at + ", \"ctCost\"", "every cost must be at least 1 (a unit that pays nothing would act again at once)");
  if (db.rules.ap.on)
    return c.fail("rules", "\"turnOrder\": \"speed\" cannot be combined with \"actionPoints\" (a side's shared pool needs the whole side to act together)");
  if (db.rules.production)
    return c.fail("rules", "\"turnOrder\": \"speed\" cannot be combined with \"production\" (building units has no place in a turn that belongs to one unit)");
  sp.on = true;
  db.rules.turnOrder = TurnOrder::Speed;
  return true;
}

bool readCharge(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, const char* what, ChargeDef& out) {
  out = ChargeDef();
  const bool any = e.has("chargeSpeed") || e.has("chargeTargets") || e.has("interruptedBy");
  if (!any) return true;
  if (!db.rules.speed.on)
    return c.fail(where, "\"chargeSpeed\", \"chargeTargets\" and \"interruptedBy\" need \"rules\": { \"turnOrder\": \"speed\" } in game.json");
  if (!e.has("chargeSpeed"))
    return c.fail(where, "\"chargeTargets\" and \"interruptedBy\" go with \"chargeSpeed\" (the " + std::string(what) + " does not charge without it)");
  if (!c.integerIn(e, "chargeSpeed", where, 1, 1000, out.speed)) return false;
  if (e.has("chargeTargets")) {
    std::string t;
    if (!c.str(e, "chargeTargets", where, t)) return false;
    if (t == "unit") out.aim = ChargeDef::Aim::Unit;
    else if (t == "cell") out.aim = ChargeDef::Aim::Cell;
    else return c.fail(where, "\"chargeTargets\" must be \"unit\" (the action follows the unit) or \"cell\" (it hits the square, whoever is there then)");
  }
  if (e.has("interruptedBy")) {
    if (!c.stringList(e, "interruptedBy", where, out.interruptIds)) return false;
    for (const std::string& id : out.interruptIds) {
      const int cond = db.findCondition(id);
      if (cond < 0) return c.fail(where, "\"interruptedBy\" names unknown condition " + q(id) + " (list it in data/conditions.json)");
      out.interrupt.push_back(cond);
    }
  }
  return true;
}

}  // namespace ote
