// Loaders for difficulties (docs/DIFFICULTY.md): game.json "difficulties", a map's "difficulty" and the unit / side keys.
#pragma once
#include "core/data/database.h"
#include "core/data/loader_util.h"

namespace ote {

// game.json "difficulties" (after "stats" and "roles"). A game without the key keeps an empty list.
bool loadDifficulties(const LoadCtx& c, const Json& root, Database& db);
// A map's "difficulty": the map's own numbers per difficulty id (after the game is loaded).
bool readMapDifficulty(const LoadCtx& c, const Database& db, const Json& root, MapDef& m);
// A unit entry's "difficulty" (the ids it appears on) and "difficultyShift" (false = never shifted); also valid on addUnit.
bool readUnitDifficulty(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, MapUnitDef& u);

// The story condition { "difficulty": "hard" } / { "difficulty": { "atLeast": "normal" } } / { "atMost": ... } of an event
// or a campaign scene: `index` = list position, `op` = Eq / Ge / Le.
bool readDifficultyCondition(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, int& index, CmpOp& op);

}  // namespace ote
