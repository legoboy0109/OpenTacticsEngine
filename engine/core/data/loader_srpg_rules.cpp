// game.json "rules.rescue" and "rules.promotion", and the "pairBonus" key on unit types (docs/SRPG.md "Rescue
// and pair up", "Promotion"). Two owners share this file, each its own functions: R1 (loadRescueRules,
// readUnitPairBonus) and P1 (loadPromotionRules).
// STUB (step F1): a game without the blocks loads as before; one that has them is refused.
#include "core/data/loader_events.h"
#include "core/data/loader_srpg.h"
#include "core/data/loader_util.h"

namespace ote {

// ---- rescue and pair up (step R1) ----

bool loadRescueRules(const LoadCtx& c, const Json& rules, Database& db) {
  db.rules.rescue = RescueRules();
  if (rules.has("rescue")) return c.fail("rules.rescue", "is not supported by this build yet");
  return true;
}

bool readUnitPairBonus(const LoadCtx& c, const Database&, const Json& e, const std::string& where, UnitTypeDef&) {
  if (e.has("pairBonus")) return c.fail(where, "\"pairBonus\" is not supported by this build yet");
  return true;
}

// ---- promotion (step P1) ----

bool loadPromotionRules(const LoadCtx& c, const Json& rules, Database& db) {
  db.rules.promotion = PromotionRules();
  if (!rules.has("promotion")) return true;
  const Json& p = rules["promotion"];
  if (!p.isObject()) return c.fail("rules.promotion", "must be an object such as { \"levelAfter\": \"reset\" }");
  for (const Json::Member& m : p.members())
    if (m.first != "levelAfter") return c.fail("rules.promotion", "unknown key \"" + m.first + "\"");
  if (p.has("levelAfter")) {
    const std::string v = p["levelAfter"].isString() ? p["levelAfter"].asString() : "";
    if (v != "reset" && v != "keep") return c.fail("rules.promotion", "\"levelAfter\" must be \"reset\" or \"keep\"");
    db.rules.promotion.keepLevel = v == "keep";
  }
  return true;
}

}  // namespace ote
