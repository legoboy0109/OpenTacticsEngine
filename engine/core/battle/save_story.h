// The story part of a battle save (switches, variables, events that have run, current objectives)
// and the ids of units. Used by save.cpp; the rules are in docs/SAVES.md.
#pragma once
#include "core/base/json.h"
#include "core/battle/battle.h"
#include "core/battle/save_reader.h"

namespace ote {

// Adds the "story" key to a battle save, only for games that have events, objectives, switches or
// variables (saves of other games stay exactly as they were).
void saveStoryState(const Battle& b, Json& out);

// Reads it back into `b` (a copy of the battle; the caller only keeps it when everything checked
// out). A game with story state needs the key, a game without must not have it.
bool restoreStoryState(savejson::Reader& rd, const Json& saved, Battle& b);

// A unit's story id from its save entry ("id", optional); empty when there is none.
bool readUnitStoryId(savejson::Reader& rd, const Json& unit, const std::string& where, std::string& out);

}  // namespace ote
