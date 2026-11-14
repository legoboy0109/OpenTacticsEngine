// Everything that belongs to one loaded game: data, Lua host and (once a
// map is started) the battle. Heap-allocated and never moved because
// Battle and Formulas keep pointers into the Database.
#pragma once
#include <string>

#include "core/base/json.h"
#include "core/battle/battle.h"
#include "core/data/database.h"
#include "core/script/formulas.h"

namespace ote {

class GameSession {
 public:
  GameSession() = default;
  GameSession(const GameSession&) = delete;
  GameSession& operator=(const GameSession&) = delete;

  // Loads game.json + data + scripts. On failure err explains what is wrong.
  bool load(const std::string& gameDir, std::string& err);
  // Loads a map, builds the battle and runs Battle::start (events -> startEvents).
  bool startMap(const std::string& mapId, unsigned seed, std::string& err);

  // Loads `mapId` and puts a saved battle state (core/battle/save.h) on it, without
  // starting a turn. Everything is checked first; on failure nothing changes.
  bool resume(const std::string& mapId, const Json& state, std::string& err);

  Database db;
  Formulas formulas;
  MapDef map;
  Battle battle;
  std::vector<Event> startEvents;
  std::string dir;
};

}  // namespace ote
