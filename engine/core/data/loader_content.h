// Loaders for the data/*.json files. Call order matters because of cross
// references: weapons -> items -> unit types -> terrain -> structures -> checkVersusKeys.
#pragma once
#include "core/data/database.h"

namespace ote {

bool loadWeapons(const std::string& path, Database& db, std::string& err);
// Optional data/items.json (docs/ITEMS.md); needs the rules, stats, resources and weapons.
bool loadItems(const std::string& path, Database& db, std::string& err);
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

class LoadCtx;
class Json;
// The side index of a map side id, -1 if there is none (map_loader.cpp).
int findMapSide(const MapDef& m, const std::string& id);
// A unit entry of a map or of the addUnit command, in two halves (map_loader.cpp): what it is and
// where (checks type, side, position inside the map; `where` is extended to name the unit),
// then its stats and items.
bool readUnitHead(const LoadCtx& c, const Database& db, const MapDef& m, const Json& e, std::string& where,
                  MapUnitDef& u);
bool readUnitBody(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, MapUnitDef& u);
// A list of items handed out by data ("items" of a unit type or a map unit, "storage" of a map side):
// ids or { "item", "uses", "drop" }. A missing key gives an empty list. maxCount < 0 = any length.
bool readItemGrants(const LoadCtx& c, const Database& db, const Json& o, const char* key, const std::string& where,
                    bool allowDrop, int maxCount, std::vector<ItemGrant>& out);

}  // namespace ote
