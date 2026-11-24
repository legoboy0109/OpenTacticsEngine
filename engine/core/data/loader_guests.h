// Loader of guests (docs/SRPG.md "Guests"): a map unit's (or addUnit's) "guest": true. The story command "guestJoins" is read in
// loader_story_commands.cpp.
#pragma once
#include "core/data/database.h"
#include "core/data/loader_util.h"

namespace ote {

// "guest": true on a map unit or an addUnit: refused on a computer side (its units are the computer's already) and in a game with
// "rules": { "actionPoints" } (one shared pool pays for the side's turn, which a guest would spend). `u.guest` stays false without the key.
bool readUnitGuest(const LoadCtx& c, const Database& db, const MapDef& m, const Json& e, const std::string& where, MapUnitDef& u);

}  // namespace ote
