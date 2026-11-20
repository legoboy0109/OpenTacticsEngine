// Small rule switches of game.json "rules": "research" { "needsStructure" }, "fog" { "ambush" }, "hitRoll" and "display" { "enemyRanges" }.
// Both are optional and a game without them keeps its save fingerprint, but note (docs/ECONOMY.md) that research
// no longer needs a building unless needsStructure is on.
#include "core/data/loader_content.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {

// Refuses keys other than `allowed` (a typo must not silently do nothing).
bool onlyKey(const LoadCtx& c, const Json& o, const std::string& where, const char* allowed) {
  for (const Json::Member& m : o.members())
    if (m.first != allowed)
      return c.fail(where, "unknown key \"" + m.first + "\" (expected \"" + allowed + "\")");
  return true;
}

}  // namespace

bool loadOptionRules(const LoadCtx& c, const Json& rules, Database& db) {
  const Json* blk;
  if (!c.object(rules, "research", "rules", blk)) return false;
  if (rules.has("research")) {
    if (!onlyKey(c, *blk, "rules.research", "needsStructure")) return false;
    if (!c.boolean(*blk, "needsStructure", "rules.research", db.rules.researchNeedsStructure)) return false;
  }
  if (!c.object(rules, "fog", "rules", blk)) return false;
  if (rules.has("fog")) {
    if (!onlyKey(c, *blk, "rules.fog", "ambush")) return false;
    std::string ambush = "endsTurn";
    if (!c.str(*blk, "ambush", "rules.fog", ambush)) return false;
    if (ambush == "endsTurn") db.rules.ambush = AmbushRule::EndsTurn;
    else if (ambush == "canAct") db.rules.ambush = AmbushRule::CanAct;
    else return c.fail("rules.fog", "\"ambush\" must be \"endsTurn\" or \"canAct\", not \"" + ambush + "\"");
  }
  if (!c.object(rules, "display", "rules", blk)) return false;
  if (rules.has("display")) {
    if (!onlyKey(c, *blk, "rules.display", "enemyRanges")) return false;
    if (!c.boolean(*blk, "enemyRanges", "rules.display", db.enemyRanges)) return false;
  }
  // "rules.hitRoll": "single" (default, one random number per strike) or "average2" (docs/SRPG.md "The hit roll").
  if (rules.has("hitRoll")) {
    std::string roll = "single";
    if (!c.str(rules, "hitRoll", "rules", roll)) return false;
    if (roll == "average2") db.rules.hitRollAverage = true;
    else if (roll != "single") return c.fail("rules", "\"hitRoll\" must be \"single\" or \"average2\", not \"" + roll + "\"");
  }
  return true;
}

}  // namespace ote
