// A game on disk with difficulties (docs/DIFFICULTY.md), built on the campaign fixture: its "skirmish" map is replaced by
// an arena with a human hero and a computer side that has a level-3 hero ("foe"), a boss that is never shifted, a unit
// with an exact attack, one unit for hard only, one for easy only; events that read the difficulty.
//
//   hero: hp 20 atk 6 def 3 mov 4, growths hp 100 atk 100 (the shifted foe grows by exactly the levels it is shifted)
//   difficulties: easy (enemy -2 levels, xp 150%, resources 150%), normal, hard (+2 levels, def +1, x110, xp 50%, resources 50%)
#pragma once
#include "campaign_run.h"

namespace difftest {

using namespace ote;

inline const char* kDifficulties = R"("difficulties": { "default": "normal", "change": "lowerOnly", "list": [
  { "id": "easy", "name": "Easy", "description": "Gentler foes.", "enemy": { "levels": -2 }, "xpPercent": 150, "resourcePercent": 150 },
  { "id": "normal", "name": "Normal" },
  { "id": "hard", "name": "Hard", "description": "Stronger foes.",
    "enemy": { "levels": 2, "stats": { "def": 1 }, "statsPercent": 110 }, "xpPercent": 50, "resourcePercent": 50 } ] })";

inline std::string gameJson(const std::string& diffs = kDifficulties) {
  std::string g = camptest::kGameJson;
  const std::string at = "\"rules\": {";
  if (!diffs.empty()) g.insert(g.find(at), diffs + ",\n  ");
  return g;
}

inline std::string arenaUnits(const std::string& extra = "") {
  return R"([ {"id":"eirik","type":"hero","side":"blue","x":0,"y":1,"name":"Eirik"},
      {"id":"foe","type":"hero","side":"red","x":7,"y":0,"level":3,"name":"Foe"},
      {"id":"boss","type":"brute","side":"red","x":7,"y":1,"difficultyShift":false},
      {"id":"exact","type":"grunt","side":"red","x":7,"y":2,"stats":{"atk":20}},
      {"id":"hardonly","type":"grunt","side":"red","x":6,"y":0,"difficulty":["hard"]},
      {"id":"easyonly","type":"grunt","side":"red","x":6,"y":2,"difficulty":["easy"]})" + extra + "]";
}

inline const char* kArenaEvents = R"([ {"id":"start","when":{"battle":"start"},"do":[
  {"if":[{"difficulty":"hard"}],"then":[{"setSwitch":"bridge_cut","to":true}]},
  {"if":[{"difficulty":{"atLeast":"normal"}}],"then":[{"setSwitch":"met_wanderer","to":true}]},
  {"if":[{"difficulty":{"atMost":"normal"}}],"then":[{"setVariable":"kills","to":5}]},
  {"addUnit":{"id":"reinf","type":"grunt","side":"red","x":6,"y":1,"difficulty":["hard"]}},
  {"giveResource":{"gold":100},"side":"blue"} ]} ])";

// The campaign fixture's game with difficulties and the arena. `mapJson` replaces the arena map when given.
struct Game {
  camptest::Game g;
  std::string folder;

  bool load(const std::string& name, const std::string& diffs = kDifficulties, const std::string& mapJson = "",
            const std::string& campaign = camptest::kCampaignJson, std::string* errOut = nullptr) {
    folder = name;
    if (!g.load(name, "")) return false;  // the plain files first (no campaign: it may read the difficulty); ours follow
    const std::string arena = mapJson.empty() ? camptest::mapJson(8, 3, camptest::sides("0", "[]"), arenaUnits(), kArenaEvents) : mapJson;
    bool ok = camptest::Game::write(g.dir + "/game.json", gameJson(diffs)) && camptest::Game::write(g.dir + "/maps/skirmish.json", arena);
    if (ok && !campaign.empty()) ok = camptest::Game::write(g.dir + "/data/campaign.json", campaign);
    if (!ok) return false;
    std::string err;
    g.db = Database();
    ok = loadGame(g.dir, g.db, err) && g.formulas.init(g.db, err);
    if (!ok && errOut) *errOut = err;
    else if (!ok) std::printf("  load: %s\n", err.c_str());
    return ok;
  }
  std::string loadError(const std::string& name, const std::string& diffs, const std::string& mapJson = "") {
    std::string err;
    if (!load(name, diffs, mapJson, camptest::kCampaignJson, &err)) return err;
    MapDef map;  // maps load on their own: the map's keys are checked here
    return loadMap(g.db, "skirmish", map, err) ? "" : err;
  }
  // The arena battle at a difficulty (by id; "" = leave it to the default), started.
  bool arena(Battle& b, std::vector<Event>& ev, const char* difficulty, uint32_t seed = 1) {
    MapDef map;
    std::string err;
    if (!loadMap(g.db, "skirmish", map, err)) {
      std::printf("  loadMap: %s\n", err.c_str());
      return false;
    }
    b.setDifficulty(difficulty[0] ? g.db.difficulties.find(difficulty) : -1);
    if (!b.init(g.db, map, g.formulas, seed, err)) {
      std::printf("  init: %s\n", err.c_str());
      return false;
    }
    b.start(ev);
    return true;
  }
};

}  // namespace difftest
