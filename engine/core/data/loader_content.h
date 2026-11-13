// Loaders for the data/*.json files. Call order matters because of cross
// references: weapons -> unit types -> terrain -> checkVersusKeys.
#pragma once
#include "core/data/database.h"

namespace ote {

bool loadWeapons(const std::string& path, Database& db, std::string& err);
bool loadUnitTypes(const std::string& path, Database& db, std::string& err);
bool loadTerrain(const std::string& path, Database& db, std::string& err);
// Weapons load before unit types, so "versus" keys are checked afterwards.
bool checkVersusKeys(const std::string& weaponsPath, const Database& db, std::string& err);

}  // namespace ote
