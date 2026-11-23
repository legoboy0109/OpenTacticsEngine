// Everything that belongs to one loaded game: data, Lua host and (once a
// map is started) the battle. Heap-allocated and never moved because
// Battle and Formulas keep pointers into the Database.
#pragma once
#include <memory>
#include <string>

#include "core/base/json.h"
#include "core/battle/battle.h"
#include "core/data/database.h"
#include "campaign/campaign_run.h"
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
  // A free battle of a map with a preparation screen (docs/EVENTS.md "Preparation"): loads the map and opens the screen's
  // state in `freePrep`; the battle (not started) shows the deployment as it is now. rebuildFreePrep() makes it again after a
  // change; startPreparedMap() starts the real battle from the final choice.
  bool startMapPrep(const std::string& mapId, unsigned seed, std::string& err);
  bool rebuildFreePrep(unsigned seed, std::string& err);
  bool startPreparedMap(unsigned seed, std::string& err);

  // Loads `mapId` and puts a saved battle state (core/battle/save.h) on it, without
  // starting a turn. Everything is checked first; on failure nothing changes.
  bool resume(const std::string& mapId, const Json& state, std::string& err);

  // Factions (docs/ECONOMY.md "Factions"): the first side of `m` that a person controls, whose faction the map leaves to "choose", and that
  // has no pick yet; -1 = none left. applyFactionPicks() puts the picks into a freshly loaded map (startMap and startMapPrep do it themselves).
  int nextFactionPick(const MapDef& m) const;
  void applyFactionPicks(MapDef& m) const;
  // Per side of the map being started: the faction index a person picked, -1 = not yet. Cleared by the App when a new battle is asked for.
  std::vector<int> factionPicks;
  // Commanders (docs/COMMANDERS.md): the same for "commander": "choose". commanderChoices() lists the commanders the side may pick (its faction's list, else all).
  int nextCommanderPick(const MapDef& m) const;
  std::vector<int> commanderChoices(const MapDef& m, int side) const;
  void applyCommanderPicks(MapDef& m) const;
  std::vector<int> commanderPicks;

  Database db;
  Formulas formulas;
  MapDef map;
  Battle battle;
  std::vector<Event> startEvents;
  // The preparation screen about to open was restored from a save (a save made on the screen): the `beforePrep` events that already played
  // before it run again without being shown (docs/EVENTS.md "Before the preparation screen"). Set by the App before it builds the scene.
  bool prepResumed = false;
  // Dialogue choices wait for a person (Battle::setAskChoices): on except in --autoplay, where the first answer is taken.
  bool askChoices = false;
  std::string dir;
  // docs/DIFFICULTY.md: the difficulty a free battle is built with (an id; empty = the game's default). Set by the App from --difficulty or the choice screen.
  std::string difficulty;
  int testLevel = 0;  // --test-level (docs/DIFFICULTY.md "Test level"): 0 = off, -1 = auto, N; given to every battle built here
  std::unique_ptr<Playthrough> freePrep;  // the preparation screen of a free battle (a scratch playthrough: the map's own characters as its roster, the side's storage and funds); else null
  // The story being played, when one is (null for a free battle and for games without a campaign).
  // Its chapter battle is `battle` above; it is set up by the App, not by startMap / resume.
  std::unique_ptr<CampaignRun> campaign;
};

}  // namespace ote
