// Shared by prep.cpp and prep_items.cpp only.
#pragma once
#include "core/campaign/prep.h"

namespace ote {

// A placed unit the player chooses about: named, on the prep side, not in the map's "fixed" list.
bool prepIsCandidate(const PrepDef& prep, const MapUnitDef& u);
// The structure kind standing on `cell` (-1 none).
int prepStructureAt(const Database& db, const MapDef& map, int cell);
// A roster entry for a unit the map places (what the roster would hold had it fought a chapter already).
RosterUnit prepRosterEntry(const Database& db, const MapDef& map, const MapUnitDef& u);
// Every candidate the map places that the roster lacks enters the roster.
void prepAddMapUnits(const Database& db, Playthrough& pt, const MapDef& map);

}  // namespace ote
