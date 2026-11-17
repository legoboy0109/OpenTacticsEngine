// Loader side of a map's preparation screen (docs/EVENTS.md "Preparation").
#pragma once
#include <vector>

#include "core/data/database.h"

namespace ote {

class LoadCtx;
class Json;

// Reads the map file's optional "prep" block into m.prep. Needs the map's sides, structures and units loaded;
// `structureAt` is the per-cell structure index the structure loader made (-1 = none).
bool loadMapPrep(const LoadCtx& c, const Database& db, const Json& root, MapDef& m, const std::vector<int>& structureAt);

}  // namespace ote
