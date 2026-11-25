// The battle scene's art loader (loader_battle_art.cpp), called by loadGraphics and loadMap.
#pragma once
#include "core/data/database.h"

namespace ote {

class LoadCtx;
class Json;

// [column, row] with whole numbers >= 0 (loader_unit_frames.cpp); `key` names it in the message.
bool readArtCell(const LoadCtx& c, const Json& j, const std::string& where, const char* key, SpriteCell& out);
// The w x h block of cells starting at `at` must lie inside the sheet.
bool checkCellsInside(const LoadCtx& c, const SheetDef& sh, SpriteCell at, int w, int h, const std::string& where);
// Frames per direction on the screen ("walk" and the poses; docs/GRAPHICS.md "Walking", "Poses and facing"). `name` is the key, for
// the messages ("<where> walk: ..."); `standSheet` is the sheet used when the object names none.
bool readWalk(const LoadCtx& c, const Database& db, const Json& j, const std::string& where, int standSheet, WalkLook& out,
              const char* name = "walk");
// A unit picture's "facing", "attack", "hurt" and "ko" into out.poses (left null when the object has none of them).
bool readUnitPoses(const LoadCtx& c, const Database& db, const Json& j, const std::string& where, int standSheet, Look& out);
// A map file's "backdrop" (loader_map_backdrop.cpp, docs/VIEWS.md "Backdrop"); needs the sheets, the styles and the map's size.
bool readMapBackdrop(const LoadCtx& c, const Database& db, const Json& root, MapDef& m);

// data/graphics.json "battle" (docs/BATTLE_SCENE.md); needs the sheets, unit types, characters, weapons and terrain.
bool readBattleArt(const LoadCtx& c, Database& db, const Json& root);
// data/graphics.json "battle" > "stage" (loader_battle_stage.cpp; docs/BATTLE_SCENE.md "Fixed stage").
bool readBattleStage(const LoadCtx& c, const Json& battle, BattleStage& out);
// data/graphics.json "backgrounds" (story scenes, docs/GRAPHICS.md "Backgrounds"); needs the sheets.
bool readSceneBackgrounds(const LoadCtx& c, Database& db, const Json& root);
// data/graphics.json "badges" (loader_badges.cpp, docs/GRAPHICS.md "Unit badges"); needs the sheets, unit types and characters.
bool readBadges(const LoadCtx& c, Database& db, const Json& root);
// data/graphics.json "markers" (loader_badges.cpp, docs/GRAPHICS.md "Map markers"); needs the sheets.
bool readMarkers(const LoadCtx& c, Database& db, const Json& root);
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
