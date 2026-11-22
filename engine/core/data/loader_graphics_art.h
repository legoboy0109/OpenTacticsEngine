// The battle scene's art loader (loader_battle_art.cpp), called by loadGraphics and loadMap.
#pragma once
#include "core/data/database.h"

namespace ote {

class LoadCtx;
class Json;

// data/graphics.json "battle" (docs/BATTLE_SCENE.md); needs the sheets, unit types, characters, weapons and terrain.
bool readBattleArt(const LoadCtx& c, Database& db, const Json& root);
// data/graphics.json "battle" > "stage" (loader_battle_stage.cpp; docs/BATTLE_SCENE.md "Fixed stage").
bool readBattleStage(const LoadCtx& c, const Json& battle, BattleStage& out);
// data/graphics.json "backgrounds" (story scenes, docs/GRAPHICS.md "Backgrounds"); needs the sheets.
bool readSceneBackgrounds(const LoadCtx& c, Database& db, const Json& root);
// data/graphics.json "tilesets" (docs/GRAPHICS.md "Tile layer"); needs the sheets.
bool readTilesets(const LoadCtx& c, Database& db, const Json& root);
// A map file's "tileLayer" (loader_map_tiles.cpp); needs the tilesets and the map's size.
bool readMapTileLayer(const LoadCtx& c, const Database& db, const Json& root, MapDef& m);
// A map file's "battleBackground".
bool readMapBattleTables(const LoadCtx& c, const Database& db, const Json& root, MapDef& m);  // "battleBackgrounds" / "battlePlatforms"
bool readMapWeather(const LoadCtx& c, const Json& root, MapDef& m);  // "weather" (loader_weather.cpp)
constexpr int kMaxMapExits = 64;
bool readMapExits(const LoadCtx& c, const Json& root, MapDef& m);    // "exits" (loader_exits.cpp)
bool readMapBattleBackground(const LoadCtx& c, const Database& db, const Json& root, MapDef& m);

}  // namespace ote
