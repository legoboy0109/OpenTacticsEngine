// Loads a game folder (docs/GAME_FORMAT.md) into a Database / MapDef.
// All problems are reported with file + what is wrong, since creators will
// read these messages.
#pragma once
#include "core/data/database.h"

namespace ote {

// gameDir: folder containing game.json. Validates every cross reference
// (unknown stat/terrain/weapon ids are errors, not silently ignored).
bool loadGame(const std::string& gameDir, Database& out, std::string& err);

// Loads one map listed in db.maps by id.
bool loadMap(const Database& db, const std::string& mapId, MapDef& out, std::string& err);
// The same without the map's events, objectives and preparation (they are not read at all): the map a story `showMap` shows
// (docs/EVENTS.md "Showing another map"). Not reading the events keeps two maps that show each other from loading each other forever.
bool loadMapShape(const Database& db, const std::string& mapId, MapDef& out, std::string& err);

// Parses "#rrggbb" into 0xRRGGBB; returns def on bad input.
uint32_t parseColor(const std::string& text, uint32_t def);

}  // namespace ote
