// Loader of downed units and revive (docs/SRPG.md "Downed units and revive"). Every refusal names the file, the entry and the key.
#include "core/data/loader_downed.h"

#include <cctype>

namespace ote {

namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

const char* const kNeedsRules = "needs \"rules\": { \"downed\": {} } in game.json (without it a defeated unit leaves the board and there is nobody to revive)";

// The shared reader: a whole number 1..999, "full", { "percent": 1..100 } or (when `exprFile` is given) a formula. `key` names the key in
// the messages.
bool readAmount(const LoadCtx& c, Database* db, const Json& v, const std::string& where, const std::string& key, const char* exprFile,
                ReviveAmount& out) {
  out = ReviveAmount();
  const std::string bad = q(key) + " must be " + reviveAmountForms(exprFile != nullptr);
  if (v.isString() && v.asString() == "full") {
    out.fixed = kHealFull;
  } else if (v.isString()) {
    if (!exprFile || !db || v.asString().empty()) return c.fail(where, bad);
    bool word = true;  // a bare word ("most") is a slip, not a formula (as for a heal)
    for (char ch : v.asString()) word = word && (std::isalpha((unsigned char)ch) || ch == '_');
    if (word) return c.fail(where, bad + " (" + q(v.asString()) + " is not one: a formula reads the units, such as \"b.hp_max / 4\")");
    out.expr = db->addExpr(v.asString(), std::string(exprFile) + where + ": " + q(key));
  } else if (v.isObject()) {
    if (v.size() != 1 || !v.has("percent") || !LoadCtx::isWhole(v["percent"]) || v["percent"].asInt() < 1 || v["percent"].asInt() > 100)
      return c.fail(where, bad);
    out.percent = v["percent"].asInt();
  } else if (LoadCtx::isWhole(v) && v.asInt() >= 1 && v.asInt() <= 999) {
    out.fixed = v.asInt();
  } else {
    return c.fail(where, bad);
  }
  return true;
}

}  // namespace

const char* reviveAmountForms(bool formula) {
  return formula ? "a whole number of HP from 1 to 999, \"full\", { \"percent\": 1 to 100 } or a formula"
                 : "a whole number of HP from 1 to 999, \"full\" or { \"percent\": 1 to 100 }";
}

bool loadDownedRules(const LoadCtx& c, const Json& rules, Database& db) {
  DownedRules& d = db.rules.downed;
  d = DownedRules();
  if (!rules.has("downed")) return true;
  const Json* blk;
  if (!c.object(rules, "downed", "rules", blk, true)) return false;
  const std::string at = "rules.downed";
  if (!c.onlyKeys(*blk, {"afterBattle", "removeAfter"}, at)) return false;
  if (blk->has("afterBattle")) {
    const Json& v = (*blk)["afterBattle"];
    if (!v.isString() || (v.asString() != "return" && v.asString() != "fallen"))
      return c.fail(at, "\"afterBattle\" must be \"return\" (a unit still downed when the battle ends is back for the next one, the default) or "
                        "\"fallen\" (it counts as fallen, and the campaign's \"fallen\" rule decides)");
    d.fallenAfterBattle = v.asString() == "fallen";
  }
  if (blk->has("removeAfter")) {
    const Json& v = (*blk)["removeAfter"];
    if (!LoadCtx::isWhole(v) || v.asInt() < 0 || v.asInt() > 99)
      return c.fail(at, "\"removeAfter\" must be a whole number from 0 to 99 (the turns of its side a downed unit lies on the board before it is "
                        "gone for good; 0 = never)");
    d.removeAfter = v.asInt();
  }
  d.on = true;
  return true;
}

bool readAbilityRevive(const LoadCtx& c, Database& db, const Json& v, const std::string& where, AbilityDef& a) {
  a.effect = AbilityEffect::Revive;
  if (!db.rules.downed.on) return c.fail(where, std::string("\"revive\" ") + kNeedsRules);
  ReviveAmount r;
  if (!readAmount(c, &db, v, where, "revive", "data/abilities.json: ", r)) return false;
  a.amount = r.fixed;
  a.healPercent = r.percent;
  a.healExpr = r.expr;
  return true;
}

bool checkAbilityRevive(const LoadCtx& c, const Json& e, const std::string& where, const AbilityDef& a) {
  if (a.effect != AbilityEffect::Revive) return true;
  if (a.target != AbilityTarget::Ally && a.target != AbilityTarget::Unit)
    return c.fail(where, "\"revive\" needs \"target\" \"ally\" (downed units of the user's team) or \"unit\" (any downed unit)");
  if (e.has("inflict")) return c.fail(where, "\"revive\" cannot go with \"inflict\" (give the condition with another ability once the unit stands)");
  if (a.charge.on()) return c.fail(where, "\"revive\" cannot be charged (\"chargeSpeed\"): it acts at once");
  return true;
}

bool readConditionAutoRevive(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, ConditionDef& d) {
  d.autoRevive = ReviveAmount();
  if (!e.has("autoRevive")) return true;
  if (!db.rules.downed.on) return c.fail(where, std::string("\"autoRevive\" ") + kNeedsRules);
  return readAmount(c, nullptr, e["autoRevive"], where, "autoRevive", nullptr, d.autoRevive);
}

bool readItemRevive(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, ItemDef& it) {
  it.revive = ReviveAmount();
  if (!e.has("revive")) return true;
  if (it.kind != ItemKind::Usable) return c.fail(where, "\"revive\" belongs to items of kind \"usable\" (a staff lists it in \"effects\")");
  if (!db.rules.downed.on) return c.fail(where, std::string("\"revive\" ") + kNeedsRules);
  if (!readAmount(c, nullptr, e["revive"], where, "revive", nullptr, it.revive)) return false;
  for (const char* other : {"heal", "raise", "cure", "inflict", "promotes", "opens", "light"})
    if (e.has(other)) return c.fail(where, std::string("\"revive\" cannot go with ") + q(other) + " (a revive item only stands a downed unit up)");
  if (!e.has("target") || !e["target"].isString() || e["target"].asString() != "ally")
    return c.fail(where, "\"revive\" needs \"target\": \"ally\" (it is used on a downed neighbour; a downed unit cannot use items)");
  return true;
}

}  // namespace ote
