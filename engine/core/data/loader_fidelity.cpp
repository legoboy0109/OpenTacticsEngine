// Loader pieces of the "fidelity" options (docs/SRPG.md "Experience: options", "Abilities: experience and conditions"):
// the extra keys of rules.progression and the two extra keys of an ability. Each is optional; a game that writes none of
// them loads exactly as before.
#include "core/data/loader_conditions.h"
#include "core/data/loader_content.h"
#include "core/data/loader_util.h"
#include "core/data/staff.h"

namespace ote {

bool loadProgressionExtras(const LoadCtx& c, const Json& block, Database& db) {
  RulesConfig& r = db.rules;
  const bool any = block.has("xpNoDamage") || block.has("computerGains");
  if (!any) return true;
  if (!r.progression) return c.fail("rules.progression", "\"xpNoDamage\" and \"computerGains\" need experience: set \"enabled\": true");
  if (block.has("computerGains") && !c.boolean(block, "computerGains", "rules.progression", r.computerGains)) return false;
  if (block.has("xpNoDamage")) {
    const Json& x = block["xpNoDamage"];
    if (x.isString()) r.xpNoDamageExpr = db.addExpr(x.asString(), "game.json: rules.progression.xpNoDamage");
    else if (LoadCtx::isWhole(x) && x.asInt() >= 0 && x.asInt() <= kMaxStaffXp) r.xpNoDamageExpr = db.addExpr(std::to_string(x.asInt()), "game.json: rules.progression.xpNoDamage");
    else return c.fail("rules.progression", "\"xpNoDamage\" must be a whole number from 0 to " + std::to_string(kMaxStaffXp) + " or a formula (text; a = the unit, b = its foe)");
  }
  return true;
}

bool readAbilityExtras(const LoadCtx& c, Database& db, const Json& e, const std::string& where, AbilityDef& a) {
  if (e.has("xp")) {
    const Json& x = e["xp"];
    if (!db.rules.progression) return c.fail(where, "\"xp\" needs experience: set \"rules\": { \"progression\": { \"enabled\": true } } in game.json");
    if (x.isString()) a.xpExpr = db.addExpr(x.asString(), "data/abilities.json: " + where + ": \"xp\"");
    else if (LoadCtx::isWhole(x) && x.asInt() >= 0 && x.asInt() <= kMaxStaffXp) a.xp = x.asInt();
    else return c.fail(where, "\"xp\" must be a whole number from 0 to " + std::to_string(kMaxStaffXp) + " or a formula (text; a = the user, b = the target)");
  }
  if (e.has("inflict")) {  // the same form a staff's and a usable item's inflict take
    InflictSpec spec;
    if (!readInflictSpec(c, db, e["inflict"], where, "", true, spec)) return false;
    if (spec.chance) {  // docs/SRPG.md "Ability conditions": a percent, or a formula (a = the user, b = the target)
      const Json& ch = *spec.chance;
      if (ch.isString() && !ch.asString().empty()) a.inflictChanceExpr = db.addExpr(ch.asString(), "data/abilities.json: " + where + ": \"inflict\": \"chance\"");
      else if (LoadCtx::isWhole(ch) && ch.asInt() >= 1 && ch.asInt() <= 100) a.inflictChance = ch.asInt() < 100 ? ch.asInt() : 0;  // 100 = always: no roll
      else return c.fail(where, "\"inflict\": \"chance\" must be a percent from 1 to 100 or a formula such as \"a.mag + 40 - b.res\" (a = the user, b = the target)");
    }
    if (a.target == AbilityTarget::Cell || a.effect == AbilityEffect::Summon || a.effect == AbilityEffect::Unlock)
      return c.fail(where, "\"inflict\" needs a target unit: \"target\" must be \"self\", \"ally\", \"enemy\" or \"unit\" (not a summon or an unlock)");
    a.inflictCond = spec.condition;
    a.inflictTurns = spec.turns;
  }
  return true;
}

}  // namespace ote
