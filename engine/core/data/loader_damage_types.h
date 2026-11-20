// Declared damage types (docs/SRPG.md "Damage types"): game.json "damageTypes" / "resistRange", a weapon's "damageType" and the
// "resist" map of a unit type or character. A game that declares no types keeps every one of these empty and nothing changes.
#pragma once
#include "core/data/database.h"
#include "core/data/loader_util.h"

namespace ote {

// game.json "damageTypes" and "resistRange" (read with the rest of game.json, before the weapons).
bool loadDamageTypes(const LoadCtx& c, const Json& root, Database& db);
// A weapon's "damageType" (an id from the list). Sets WeaponDef::damageType: the named type, else the type a "damageKind" of the same
// word names, else the first one; -1 in a game without types (where the key is refused).
bool readWeaponDamageType(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, WeaponDef& w);
// "resist": { "<type id>": whole number }. `out` gets one entry per declared type; unwritten ones are `unset` (0 for a unit type,
// kResistUnset for a character). Stays empty when the key is absent.
bool readResist(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, int unset, std::vector<int>& out);

}  // namespace ote
