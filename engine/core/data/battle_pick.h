// Which background and which platform a battle scene uses (docs/BATTLE_SCENE.md "Which background and platform"). Pure: no player code.
#pragma once
#include "core/data/database.h"

namespace ote {

// The background (index into Graphics::battle.backgrounds, -1 = none: the scene makes a plain one) for a fight whose deciding square has
// terrain `terrain` (-1 = none). Order: the map's table entry for that terrain, the map's single "battleBackground", the map table's
// "default", the game-wide terrain entry, the game's default.
int pickBattleBackground(const Database& db, const MapDef& map, int terrain);

// The platform (index into Graphics::battle.platforms, -1 = none), by the same order (a map has no single platform).
int pickBattlePlatform(const Database& db, const MapDef& map, int terrain);

}  // namespace ote
