// data/commanders.json (docs/COMMANDERS.md): optional; a game without the file has no commanders.
#pragma once
#include "core/data/loader_util.h"

namespace ote {

// Needs the skills, resources and unit types loaded (a commander's passive skills, powers and effects name them). Loaded before the factions
// (a faction may list the commanders its "choose" sides may pick).
bool loadCommanders(const std::string& path, Database& db, std::string& err);

// A map side's "commander": an id from data/commanders.json, or "choose". Sets s.commander / s.commanderChoose; refuses an unknown id, and both
// forms in a game without commanders. Absent = no commander. `where` names the side for messages.
bool readSideCommander(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, MapSideDef& s);

}  // namespace ote
