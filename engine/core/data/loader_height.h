// Loader of the height rules (docs/SRPG.md "Height rules"). Every refusal names the file, the entry and the key.
#pragma once
#include "core/data/database.h"
#include "core/data/loader_util.h"

namespace ote {

// game.json "rules"."height" (after the move types are known).
bool loadHeightRules(const LoadCtx& c, const Json& rules, Database& db);
// A weapon's "heightRange" and an area's "vertical" (weapons.json); `w` is the weapon being read. Refused in a game without rules.height.
bool readWeaponHeight(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, WeaponDef& w);
// A unit type's "jump": n | { "up": n, "down": m } (units.json). Refused in a game without rules.height.
bool readUnitJump(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, UnitTypeDef& u);

}  // namespace ote
