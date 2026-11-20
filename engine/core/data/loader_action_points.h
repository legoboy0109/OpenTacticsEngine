// Loader of the action-point system (docs/SRPG.md "Action points") and of a resource's "refill" (docs/ECONOMY.md "Refilling resources").
// Every refusal names the file, the entry and the key.
#pragma once
#include "core/data/database.h"
#include "core/data/loader_util.h"

namespace ote {

// game.json "resources": each entry's optional "refill": { "perTurn": n, "carryOver": m }. Read right after the resource list.
bool loadResourceRefills(const LoadCtx& c, const Json& root, Database& db);
// game.json "rules"."actionPoints" (after the resources are known).
bool loadActionPointRules(const LoadCtx& c, const Json& rules, Database& db);
// A weapon's "apCost" and an area's "apExtra" (weapons.json); `w` is the weapon being read. Refused in a game without actionPoints.
bool readWeaponAp(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, WeaponDef& w);
// A unit type's "ap": { "step": n, "softCap": n } (units.json). Refused in a game without actionPoints.
bool readUnitAp(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, UnitTypeDef& u);

}  // namespace ote
