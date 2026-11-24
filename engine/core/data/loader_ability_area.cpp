// An ability's "area" and its list of "inflict" entries (docs/SRPG.md "Areas and several effects for abilities"). A game that writes neither
// loads exactly as before: no area (AreaDef off) and `inflicts` holding the one entry the single "inflict" object gave.
#include "core/data/loader_ability_area.h"

#include "core/data/loader_conditions.h"
#include "core/data/loader_content.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

const int kMaxAbilityInflicts = 8;
constexpr int kMaxLevels = 999;  // as loader_height.cpp: the most an area's "vertical" may be

// One entry: { "condition", "turns", "chance" }, the same form the single object always had.
bool readOneInflict(const LoadCtx& c, Database& db, const Json& v, const std::string& where, const std::string& at, AbilityInflict& out) {
  InflictSpec spec;
  if (!readInflictSpec(c, db, v, where, at, true, spec)) return false;
  if (spec.chance) {  // docs/SRPG.md "Ability conditions": a percent, or a formula (a = the user, b = the target)
    const Json& ch = *spec.chance;
    const std::string head = at.empty() ? "\"inflict\"" : at + ": \"inflict\"";
    if (ch.isString() && !ch.asString().empty()) out.chanceExpr = db.addExpr(ch.asString(), "data/abilities.json: " + where + ": " + head + ": \"chance\"");
    else if (LoadCtx::isWhole(ch) && ch.asInt() >= 1 && ch.asInt() <= 100) out.chance = ch.asInt() < 100 ? ch.asInt() : 0;  // 100 = always: no roll
    else return c.fail(where, head + ": \"chance\" must be a percent from 1 to 100 or a formula such as \"a.mag + 40 - b.res\" (a = the user, b = the target)");
  }
  out.cond = spec.condition;
  out.turns = spec.turns;
  return true;
}

}  // namespace

bool readAbilityInflicts(const LoadCtx& c, Database& db, const Json& v, const std::string& where, AbilityDef& a) {
  a.inflicts.clear();
  if (v.isArray()) {
    if (v.size() == 0 || (int)v.size() > kMaxAbilityInflicts)
      return c.fail(where, "\"inflict\" must be one condition entry or a list of 1 to " + std::to_string(kMaxAbilityInflicts) +
                               " such as [{ \"condition\": \"protect\" }, { \"condition\": \"shell\" }]");
    for (size_t i = 0; i < v.size(); ++i) {
      AbilityInflict one;
      if (!readOneInflict(c, db, v[i], where, "entry " + std::to_string(i + 1), one)) return false;
      for (const AbilityInflict& o : a.inflicts)
        if (o.cond == one.cond)
          return c.fail(where, "\"inflict\" names " + q(db.conditions[(size_t)one.cond].id) + " twice (one entry per condition)");
      a.inflicts.push_back(one);
    }
  } else {
    AbilityInflict one;
    if (!readOneInflict(c, db, v, where, "", one)) return false;
    a.inflicts.push_back(one);
  }
  // The first entry also lives in the old single fields (srpg.h: the code that reads one condition).
  a.inflictCond = a.inflicts[0].cond;
  a.inflictTurns = a.inflicts[0].turns;
  a.inflictChance = a.inflicts[0].chance;
  a.inflictChanceExpr = a.inflicts[0].chanceExpr;
  return true;
}

bool readAbilityArea(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, AbilityDef& a) {
  a.area = AreaDef();
  if (!e.has("area")) return true;
  if (a.effect == AbilityEffect::Strike) return c.fail(where, "\"strike\" takes its area from the weapon: leave out \"area\"");
  if (a.effect != AbilityEffect::Heal && a.effect != AbilityEffect::Cure && a.effect != AbilityEffect::None)
    return c.fail(where, "\"area\" goes with \"heal\", \"cure\" (or both) or an ability with only \"inflict\"; it cannot push, pull, swap, summon, unlock, steal or refresh");
  if (a.target == AbilityTarget::Self || a.target == AbilityTarget::Cell)
    return c.fail(where, "\"area\" needs \"target\" \"ally\", \"enemy\" or \"unit\" (who in the area is affected); \"range\": [0, 0] centres it on the user");
  const Json* o;
  if (!c.object(e, "area", where, o)) return false;
  for (const Json::Member& m : o->members()) {
    if (m.first == "friendlyFire")
      return c.fail(where, "\"area\".\"friendlyFire\" is for weapons: an ability's \"target\" says who in the area is affected");
    if (m.first == "apExtra") return c.fail(where, "\"area\".\"apExtra\" is for weapons (an ability has no action-point cost of its own)");
    if (m.first != "shape" && m.first != "radius" && m.first != "length" && m.first != "vertical")
      return c.fail(where, "\"area\" has an unknown key " + q(m.first) + " (an ability's area takes shape, radius, length, vertical)" +
                               didYouMean(m.first, {"shape", "radius", "length", "vertical"}));
  }
  if (!readAreaShape(c, *o, where, a.area)) return false;
  if (o->has("vertical")) {  // as a weapon's (loader_height.cpp)
    if (!db.rules.height.on) return c.fail(where, "\"area\".\"vertical\" needs \"rules\": { \"height\": ... } in game.json");
    if (!c.integerIn(*o, "vertical", where + ", \"area\"", 0, kMaxLevels, a.area.vertical)) return false;
  }
  if (a.charge.on() && a.charge.aim == ChargeDef::Aim::Unit)
    return c.fail(where, "an ability with an \"area\" keeps the square it aims at: \"chargeTargets\" must be \"cell\" (or left out)");
  return true;
}

}  // namespace ote
