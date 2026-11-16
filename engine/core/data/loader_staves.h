// Loader side of staves (contract: docs/SRPG.md section 8): the keys of an item of kind "staff" in data/items.json.
// Messages name the file and the item: `data/items.json: item "mend_staff": "range" must be ...`.
#pragma once
#include <string>

#include "core/data/database.h"

namespace ote {

class LoadCtx;
class Json;

// Reads one staff item's own keys into `it` and a new entry of db.staves (the common keys, uses and price, are read by
// loadItems). Refuses unknown keys. Needs the stats, ranks (when "rank" is used) and skills (conditions) loaded.
bool readStaffItem(const LoadCtx& c, Database& db, const Json& e, const std::string& where, ItemDef& it);
// After loadItems: the groups of staves are appended to RankRules::groups (after the weapons', so no index moves) and
// each staff's gear.rankGroup is set. Does nothing in a game without ranks.
void addStaffRankGroups(Database& db);
// In resolveSrpgRefs (characters exist by then): "wieldedBy" ids to indices, and the conditions' references.
bool resolveStaffRefs(const std::string& root, Database& db, std::string& err);

}  // namespace ote
