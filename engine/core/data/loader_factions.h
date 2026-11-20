// data/factions.json (docs/ECONOMY.md "Factions"): optional; a game without the file has no factions.
#pragma once
#include "core/data/loader_util.h"

namespace ote {

// Needs the unit types, structures, technologies, skills, resources and rules.ai loaded (a faction names all of them).
bool loadFactions(const std::string& path, Database& db, std::string& err);

// A map side's "faction": an id from data/factions.json, or "choose". Sets s.faction / s.factionChoose; refuses an unknown
// id, and both forms in a game without factions. Absent = no faction. `where` names the side for messages.
bool readSideFaction(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, MapSideDef& s);

}  // namespace ote
