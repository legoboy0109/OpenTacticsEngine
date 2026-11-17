// The battle scene's art loader (loader_battle_art.cpp), called by loadGraphics and loadMap.
#pragma once
#include "core/data/database.h"

namespace ote {

class LoadCtx;
class Json;

// data/graphics.json "battle" (docs/BATTLE_SCENE.md); needs the sheets, unit types, characters, weapons and terrain.
bool readBattleArt(const LoadCtx& c, Database& db, const Json& root);
// data/graphics.json "backgrounds" (story scenes, docs/GRAPHICS.md "Backgrounds"); needs the sheets.
bool readSceneBackgrounds(const LoadCtx& c, Database& db, const Json& root);
// A map file's "battleBackground".
bool readMapBattleBackground(const LoadCtx& c, const Database& db, const Json& root, MapDef& m);

}  // namespace ote
