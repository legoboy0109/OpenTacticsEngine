// Loader of downed units and revive (docs/SRPG.md "Downed units and revive"): game.json "rules"."downed", an ability's "effect": { "revive" },
// a condition's "autoRevive" and a usable item's "revive". A staff's { "revive": ... } effect is read with the other staff effects
// (loader_staves.cpp). Every refusal names the file, the entry and the key; each key without rules.downed is refused.
#pragma once
#include "core/data/database.h"
#include "core/data/loader_util.h"

namespace ote {

// game.json "rules"."downed". Before the data files that use the keys below.
bool loadDownedRules(const LoadCtx& c, const Json& rules, Database& db);
// An ability's "effect": { "revive": ... }: a number (1 to 999), "full", { "percent": 1 to 100 } or a formula (a = the user, b = the unit
// revived), kept in AbilityDef::amount / healPercent / healExpr as a heal's.
bool readAbilityRevive(const LoadCtx& c, Database& db, const Json& v, const std::string& where, AbilityDef& a);
// The target / range / other-key rules of a revive ability (after "effect", "inflict" and "area" are read).
bool checkAbilityRevive(const LoadCtx& c, const Json& e, const std::string& where, const AbilityDef& a);
// A condition's "autoRevive": a number, "full" or { "percent": n } (no formula: nobody casts it when it fires).
bool readConditionAutoRevive(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, ConditionDef& d);
// A usable item's "revive": a number, "full" or { "percent": n }. It must be the item's only effect and its "target" "ally".
bool readItemRevive(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, ItemDef& it);
// The plain words of the four forms, for the refusals.
const char* reviveAmountForms(bool formula);

}  // namespace ote
