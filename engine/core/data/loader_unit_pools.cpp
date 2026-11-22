// Loaders of unit resources, ability prices, strikes and heal forms (loader_unit_pools.h; docs/SRPG.md "Unit resources", "Abilities that strike",
// "Healing by formula"). Every refusal names the file, the entry and the key.
#include "core/data/loader_unit_pools.h"

#include <cctype>

namespace ote {

namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

}  // namespace

bool loadUnitResources(const LoadCtx& c, const Json& root, Database& db) {
  db.unitResources.clear();
  if (!root.has("unitResources")) return true;
  const Json* list;
  if (!c.array(root, "unitResources", "", list)) return false;
  if ((int)list->size() > kMaxUnitResources) return c.fail("unitResources", "lists at most " + std::to_string(kMaxUnitResources) + " unit resources");
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& e = (*list)[i];
    std::string where = "unitResources[" + std::to_string(i) + "]";
    if (!e.isObject()) return c.fail(where, "must be an object such as { \"id\": \"mp\", \"max\": \"mp\" }");
    UnitResourceDef d;
    if (!c.str(e, "id", where, d.id, true)) return false;
    if (!isStoryId(d.id)) return c.fail(where, "\"id\" must be letters, digits, \"_\" and \"-\" (got " + q(d.id) + ")");
    where = "unitResources " + q(d.id);
    if (!c.onlyKeys(e, {"id", "name", "max", "start", "regen"}, where)) return false;
    if (db.findUnitResource(d.id) >= 0) return c.fail(where, "duplicate id");
    if (db.findResource(d.id) >= 0) return c.fail(where, "\"id\" is already a side resource (\"resources\"); a price could not tell them apart");
    d.name = d.id;
    if (!c.str(e, "name", where, d.name)) return false;
    if (d.name.empty() || d.name.size() > 8) return c.fail(where, "\"name\" must be 1 to 8 characters");
    if (!c.str(e, "max", where, d.statId, true)) return false;
    d.stat = db.findStat(d.statId);
    if (d.stat < 0) return c.fail(where, "\"max\" names unknown stat " + q(d.statId) + " (\"stats\")");
    if (!c.integerIn(e, "start", where, 0, 100, d.start)) return false;
    if (e.has("regen")) {
      const Json* rg;
      if (!c.object(e, "regen", where, rg)) return false;
      const std::string at = where + ", \"regen\"";
      if (!c.onlyKeys(*rg, {"perTurn", "percent"}, at)) return false;
      if (!c.integerIn(*rg, "perTurn", at, 0, 999, d.regenPerTurn) || !c.integerIn(*rg, "percent", at, 0, 100, d.regenPercent)) return false;
    }
    db.unitResources.push_back(d);
  }
  return true;
}

bool readAbilitySpend(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, AbilityDef& a) {
  a.spend.clear();
  if (!e.has("spend")) return true;
  const Json* o;
  if (!c.object(e, "spend", where, o)) return false;
  if (o->size() == 0) return c.fail(where, "\"spend\" must name at least one resource, such as { \"mp\": 6 }");
  for (const Json::Member& m : o->members()) {
    AbilitySpend s;
    s.index = db.findUnitResource(m.first);
    s.unit = s.index >= 0;
    if (!s.unit) s.index = db.findResource(m.first);
    if (s.index < 0) return c.fail(where, "\"spend\" names unknown resource " + q(m.first) + " (game.json \"resources\" or \"unitResources\")");
    if (!LoadCtx::isWhole(m.second) || m.second.asInt() < 1 || m.second.asInt() > kMaxSpendAmount)
      return c.fail(where, "\"spend\": " + q(m.first) + " must be a whole number from 1 to " + std::to_string(kMaxSpendAmount));
    s.amount = m.second.asInt();
    a.spend.push_back(s);
  }
  return true;
}

bool readAbilityHeal(const LoadCtx& c, Database& db, const Json& h, const std::string& where, AbilityDef& a) {
  a.effect = AbilityEffect::Heal;
  const char* bad = "\"heal\" must be a whole number from 1 to 999, \"full\", { \"percent\": 1 to 100 } or a formula";
  if (h.isString() && h.asString() == "full") {
    a.amount = kHealFull;
  } else if (h.isString()) {
    if (h.asString().empty()) return c.fail(where, bad);
    // A bare word ("most", "ful") is a slip, not a formula: a formula names the units ("a.mag * 2") or is a number.
    bool word = true;
    for (char ch : h.asString()) word = word && (std::isalpha((unsigned char)ch) || ch == '_');
    if (word) return c.fail(where, bad + std::string(" (\"") + h.asString() + "\" is not one: a formula reads the units, such as \"a.mag * 2\")");
    a.healExpr = db.addExpr(h.asString(), "data/abilities.json: " + where + ": \"heal\"");  // a = the user, b = the unit healed
  } else if (h.isObject()) {
    if (h.size() != 1 || !h.has("percent") || !LoadCtx::isWhole(h["percent"]) || h["percent"].asInt() < 1 || h["percent"].asInt() > 100)
      return c.fail(where, bad);
    a.healPercent = h["percent"].asInt();
  } else if (LoadCtx::isWhole(h) && h.asInt() >= 1 && h.asInt() <= 999) {
    a.amount = h.asInt();
  } else {
    return c.fail(where, bad);
  }
  return true;
}

bool resolveAbilityStrike(const LoadCtx& c, Database& db, const std::string& where, AbilityDef& a) {
  if (a.effect != AbilityEffect::Strike) return true;
  a.strike = db.findWeapon(a.strikeId);
  if (a.strike < 0) return c.fail(where, "\"strike\" names unknown weapon " + q(a.strikeId) + " (data/weapons.json)");
  const WeaponDef& w = db.weapons[(size_t)a.strike];
  if (w.charge.on()) return c.fail(where, "the weapon " + q(a.strikeId) + " has \"chargeSpeed\": a strike charges through the ability");
  // Targets come from the weapon: an area weapon aims at a square, any other at an enemy unit (the menu and the charge follow this).
  a.target = w.area.on() ? AbilityTarget::Cell : AbilityTarget::Enemy;
  a.minRange = w.minRange;
  a.maxRange = w.maxRange;
  return true;
}

}  // namespace ote
