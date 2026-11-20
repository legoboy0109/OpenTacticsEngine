// Loader of the facing rules (docs/SRPG.md "Facing"). Every refusal names the file, the entry and the key.
#pragma once
#include "core/data/database.h"
#include "core/data/loader_util.h"

namespace ote {

// game.json "rules"."facing".
bool loadFacingRules(const LoadCtx& c, const Json& rules, Database& db);
// A map unit's (or addUnit's) "facing": one of the grid's direction names (grid/facing.h). Refused in a game without rules.facing and for a name that
// is not a direction of the map's grid. `u.facing` stays -1 (the default) without the key.
bool readUnitFacing(const LoadCtx& c, const Database& db, const MapDef& m, const Json& e, const std::string& where, MapUnitDef& u);

}  // namespace ote
