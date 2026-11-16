// Character-locked weapons (contract: docs/SRPG.md "Character-locked weapons"): weapons.json "wieldedBy".
#pragma once
#include <string>

#include "core/data/database.h"

namespace ote {

class LoadCtx;
class Json;

// weapons.json entry: "wieldedBy": ["kell"]. Only the shape is checked here (characters.json loads after the weapons).
bool readWeaponWielders(const LoadCtx& c, const Json& e, const std::string& where, WeaponDef& w);
// After characters.json: turns the ids into indices; unknown ids, repeats and "no characters in this game" are errors.
bool resolveWeaponWielders(const std::string& root, Database& db, std::string& err);

}  // namespace ote
