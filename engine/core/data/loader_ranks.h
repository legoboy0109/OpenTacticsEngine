// Loaders of weapon ranks and stat caps (contract: docs/SRPG.md section 7). Both files are optional.
#pragma once
#include <string>
#include <vector>

#include "core/data/database.h"

namespace ote {

class LoadCtx;
class Json;

// data/ranks.json: before the weapons (they name ranks). Checks that the game has carried weapons. An empty ladder
// leaves the game without ranks.
bool loadRanks(const std::string& path, Database& db, std::string& err);
// data/caps.json: needs the stats.
bool loadStatCaps(const std::string& path, Database& db, std::string& err);
// weapons.json entry: "rank", "rankXp" (after "group" was read).
bool readWeaponRank(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, WeaponDef& w);
// After all weapons: fills RankRules::groups and every weapon's rankGroup.
void buildRankGroups(Database& db);
// "ranks": { "lance": "c" | 45 } into per-group weapon experience (groups not named get `unset`). Leaves `out` empty
// when the key is missing.
bool readRankMap(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, int unset,
                 std::vector<int>& out);
// units.json entry: "ranks", "caps". characters.json entry: "ranks", "capAdjust".
bool readUnitRanks(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, UnitTypeDef& u);
bool readCharacterRanks(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, CharacterDef& ch);

}  // namespace ote
