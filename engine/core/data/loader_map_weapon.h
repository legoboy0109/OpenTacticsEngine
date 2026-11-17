// Loader side of map weapons (docs/SRPG.md "Map weapons").
#pragma once
#include "core/data/database.h"

namespace ote {

class LoadCtx;
class Json;

// The "weapon", "uses", "by", "ignoreRank" and "becomes" of every structure kind, read once all kinds exist. `list` is the
// "structures" array of data/structures.json; db.structures holds its entries in the same order.
bool loadStructureWeapons(const LoadCtx& c, const Json& list, Database& db);
// A placed structure's own "uses" (map_loader.cpp calls this once the kind is known).
bool readPlacedWeapon(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, MapStructureDef& s);

}  // namespace ote
