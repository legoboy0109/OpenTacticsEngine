// Loaders for the data/*.json files. Call order matters because of cross
// references: weapons -> unit types -> terrain -> checkVersusKeys.
#pragma once
#include "core/data/database.h"

namespace ote {

bool loadWeapons(const std::string& path, Database& db, std::string& err);
bool loadUnitTypes(const std::string& path, Database& db, std::string& err);
bool loadTerrain(const std::string& path, Database& db, std::string& err);
// Optional data/structures.json (docs/STRUCTURES.md); needs move types, unit
// types and terrain loaded. A missing file means the game has no structures.
bool loadStructures(const std::string& path, Database& db, std::string& err);
// Weapons load before unit types, so "versus" keys are checked afterwards.
bool checkVersusKeys(const std::string& weaponsPath, const Database& db, std::string& err);
// Optional data/graphics.json; needs terrain, unit types and styles loaded.
// A missing file is not an error (the game then has no art).
bool loadGraphics(const std::string& path, Database& db, std::string& err);

}  // namespace ote
