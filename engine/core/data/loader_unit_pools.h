// Loaders of unit resources and ability prices (docs/SRPG.md "Unit resources (MP and the like)"), and of the other ability keys of the same step:
// "effect": { "strike": ... } and the heal forms (docs/SRPG.md "Abilities that strike", "Healing by formula").
#pragma once
#include "core/data/database.h"
#include "core/data/loader_util.h"

namespace ote {

// game.json "unitResources" (after "stats" and "resources", which it names and must not clash with).
bool loadUnitResources(const LoadCtx& c, const Json& root, Database& db);
// An ability's "spend": { "<resource id>": amount } (unit resources and side resources).
bool readAbilitySpend(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, AbilityDef& a);
// The value of an ability's "heal": a whole number, "full", { "percent": n } or a formula.
bool readAbilityHeal(const LoadCtx& c, Database& db, const Json& h, const std::string& where, AbilityDef& a);
// "strike" once the weapons are loaded: the weapon exists and does not charge by itself.
bool resolveAbilityStrike(const LoadCtx& c, Database& db, const std::string& where, AbilityDef& a);

}  // namespace ote
