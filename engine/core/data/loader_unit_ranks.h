// Loader of the unit rank table (docs/SRPG.md "Unit ranks"): game.json "rules"."progression"."levels". Every refusal names the file, the entry
// and the key.
#pragma once
#include "core/data/database.h"
#include "core/data/loader_util.h"

namespace ote {

// Reads "levels" from the progression block (`block`; the experience switch and the stat list are already known). Without the key nothing
// changes. The skill ids are kept as text until resolveUnitRankSkills, because skills.json is read after game.json.
bool loadUnitRankRules(const LoadCtx& c, const Json& block, Database& db);
// "rules"."ranks" (merit mode, docs/SRPG.md "Unit ranks"): the same table on its own counter. Call after loadUnitRankRules; refuses both together.
bool loadMeritRankRules(const LoadCtx& c, const Json& rules, Database& db);
// Turns the skill ids of the table into indices (last step of loading; `root` = the game folder). Refuses an unknown skill, naming game.json, the entry and the key.
bool resolveUnitRankSkills(const std::string& root, Database& db, std::string& err);
// A map unit's (or addUnit's) "xp": the experience it starts with. Refused in a game without a rank table (the other experience is per level).
bool readUnitXp(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, MapUnitDef& u);
// The same for "merit" (games with "rules.ranks" only).
bool readUnitMerit(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, MapUnitDef& u);

}  // namespace ote
