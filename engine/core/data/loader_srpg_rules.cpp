// game.json "rules.rescue" and "rules.promotion", and the "pairBonus" key on unit types (docs/SRPG.md "Rescue
// and pair up", "Promotion"). Two owners share this file, each its own functions: R1 (loadRescueRules,
// readUnitPairBonus) and P1 (loadPromotionRules).
// Promotion (P1) is still a stub: a game that has the block is refused.
#include "core/data/loader_events.h"
#include "core/data/loader_srpg.h"
#include "core/data/loader_util.h"

namespace ote {

// ---- rescue and pair up (step R1) ----

namespace {

// "carrierStats" entry: a whole number is added, { "percent": n } multiplies (0..1000).
bool readStatChange(const LoadCtx& c, const Json& v, const std::string& stat, StatChange& out) {
  const std::string where = "rules.rescue";
  if (v.isObject()) {
    for (const Json::Member& m : v.members())
      if (m.first != "percent")
        return c.fail(where, "\"carrierStats\".\"" + stat + "\" has unknown key \"" + m.first + "\" (only \"percent\")" + didYouMean(m.first, {"percent"}));
    if (!v.has("percent") || !LoadCtx::isWhole(v["percent"]) || v["percent"].asInt() < 0 || v["percent"].asInt() > 1000)
      return c.fail(where, "\"carrierStats\".\"" + stat + "\".\"percent\" must be a whole number from 0 to 1000");
    out.percent = v["percent"].asInt();
    return true;
  }
  if (!LoadCtx::isWhole(v))
    return c.fail(where, "\"carrierStats\".\"" + stat + "\" must be a whole number or { \"percent\": n }");
  out.add = v.asInt();
  return true;
}

// "pairBonus" of `o`: {"stat": number} -> per stat index, zero where not named; the health stat is refused.
bool readPairBonusMap(const LoadCtx& c, const Database& db, const Json& o, const std::string& where, std::vector<int>& out) {
  out.assign(db.stats.size(), 0);
  if (!c.statMap(db, o, "pairBonus", where, out)) return false;
  const std::string& hp = db.stats[(size_t)db.rules.statHp].id;
  if (o["pairBonus"].has(hp.c_str())) return c.fail(where, "\"pairBonus\" cannot change \"" + hp + "\" (the health stat)");
  return true;
}

}  // namespace

bool loadRescueRules(const LoadCtx& c, const Json& rules, Database& db) {
  RescueRules& r = db.rules.rescue;
  r = RescueRules();
  if (!rules.has("rescue")) return true;
  const Json& j = rules["rescue"];
  const std::string where = "rules.rescue";
  if (!j.isObject()) return c.fail(where, "must be an object");
  for (const Json::Member& m : j.members())
    if (m.first != "mode" && m.first != "canCarry" && m.first != "carrierStats" && m.first != "pairBonus")
      return c.fail(where, unknownKeyText(m.first, {"mode", "canCarry", "carrierStats", "pairBonus"}));
  if (!j.has("mode") || !j["mode"].isString() || (j["mode"].asString() != "carry" && j["mode"].asString() != "pairUp"))
    return c.fail(where, "\"mode\" must be \"carry\" or \"pairUp\"");
  r.mode = j["mode"].asString() == "carry" ? RescueMode::Carry : RescueMode::PairUp;

  if (j.has("canCarry")) {
    if (!j["canCarry"].isString()) return c.fail(where, "\"canCarry\" must be a string (a formula)");
    // Compiled by Formulas::init; a syntax error is reported under this label.
    r.canCarry = db.addExpr(j["canCarry"].asString(), "game.json: rules.rescue.canCarry");
  }
  if (j.has("carrierStats")) {
    const Json& m = j["carrierStats"];
    if (!m.isObject()) return c.fail(where, "\"carrierStats\" must be an object");
    r.carrierStats.assign(db.stats.size(), StatChange());
    for (const Json::Member& e : m.members()) {
      const int s = db.findStat(e.first);
      if (s < 0) return c.fail(where, "\"carrierStats\" names unknown stat \"" + e.first + "\"");
      if (s == db.rules.statHp) return c.fail(where, "\"carrierStats\" cannot change \"" + e.first + "\" (the health stat)");
      if (!readStatChange(c, e.second, e.first, r.carrierStats[(size_t)s])) return false;
    }
  }
  if (j.has("pairBonus")) {
    if (r.mode != RescueMode::PairUp) return c.fail(where, "\"pairBonus\" belongs to \"mode\": \"pairUp\"");
    if (!readPairBonusMap(c, db, j, where, r.pairBonus)) return false;
  }
  return true;
}

bool readUnitPairBonus(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, UnitTypeDef& u) {
  if (!e.has("pairBonus")) return true;
  if (db.rules.rescue.mode != RescueMode::PairUp)
    return c.fail(where, "\"pairBonus\" needs \"rules\": { \"rescue\": { \"mode\": \"pairUp\" } } in game.json");
  // Present means "replaces the game's bonus as a whole", so an empty object is a real (zero) bonus: never an empty list.
  return readPairBonusMap(c, db, e, where, u.pairBonus);
}

// ---- promotion (step P1) ----

bool loadPromotionRules(const LoadCtx& c, const Json& rules, Database& db) {
  db.rules.promotion = PromotionRules();
  if (!rules.has("promotion")) return true;
  const Json& p = rules["promotion"];
  if (!p.isObject()) return c.fail("rules.promotion", "must be an object such as { \"levelAfter\": \"reset\" }");
  for (const Json::Member& m : p.members())
    if (m.first != "levelAfter" && m.first != "classSkills" && m.first != "testAt")
      return c.fail("rules.promotion", unknownKeyText(m.first, {"levelAfter", "classSkills", "testAt"}));
  if (p.has("testAt")) {  // the range (up to the game's top level) is checked once the progression rules are read
    if (!p["testAt"].isNumber() || !c.integer(p, "testAt", "rules.promotion", db.rules.promotion.testAt) || db.rules.promotion.testAt < 1)
      return c.fail("rules.promotion", "\"testAt\" must be a whole number from 1 up to the game's top level (rules.progression.maxLevel)");
  }
  if (p.has("classSkills")) {
    const std::string v = p["classSkills"].isString() ? p["classSkills"].asString() : "";
    if (v != "keep" && v != "replace") return c.fail("rules.promotion", "\"classSkills\" must be \"keep\" (default) or \"replace\"");
    db.rules.promotion.replaceClassSkills = v == "replace";
  }
  if (p.has("levelAfter")) {
    const std::string v = p["levelAfter"].isString() ? p["levelAfter"].asString() : "";
    if (v != "reset" && v != "keep") return c.fail("rules.promotion", "\"levelAfter\" must be \"reset\" or \"keep\"");
    db.rules.promotion.keepLevel = v == "keep";
  }
  return true;
}

}  // namespace ote
