// Loaders of learning from equipment (docs/JOBS.md "Learning from equipment (ability points)"): an item's "teaches", game.json
// "rules"."abilityPoints", and the cross checks with the jobs once data/jobs.json is read.
#pragma once
#include "core/data/database.h"
#include "core/data/loader_util.h"

namespace ote {

// An item's "teaches" (a weapon or an accessory): fills Database::teachEntries (each skill / ability once) and ItemDef::teaches.
bool readItemTeaches(const LoadCtx& c, Database& db, const Json& e, const std::string& where, ItemDef& it);
// game.json "rules"."abilityPoints" (the defaults of AbilityPointRules without it).
bool loadAbilityPointRules(const LoadCtx& c, const Json& rules, Database& db);
// A map's own "abilityPoints": { "perBattle": n } (docs/JOBS.md "Learning from equipment"): replaces rules.abilityPoints.perBattle for a battle on
// this map. MapDef::apPerBattle stays -1 (the game's) without the key.
bool readMapAbilityPoints(const LoadCtx& c, const Database& db, const Json& root, MapDef& m);
// After data/jobs.json: links taught entries to the job entries of the same id, and the checks that need both (the block that does nothing, "spendJp").
bool resolveTeaching(const std::string& root, Database& db, std::string& err);

}  // namespace ote
