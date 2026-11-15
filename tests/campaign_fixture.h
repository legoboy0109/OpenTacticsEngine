// A small complete game on disk for the campaign tests (docs/EVENTS.md "Campaign"): written into the
// build directory so every test can change data/campaign.json, load it with the real loader and hash
// real files. Rules: progression (levels), items with a shared storage, gold, two switches, two variables.
//
//   units:  hero (hp 20 atk 6 def 3 mov 4, sword)   ranger (hp 12 atk 5 def 1 mov 4, bow)
//           grunt (hp 4 atk 3 def 1 mov 3, sword)    brute (hp 30 atk 12 def 6 mov 3, sword)
//   items:  potion (heals 5, 3 uses)  tonic (atk +1 for good, 1 use)  ring (accessory, hp +4)
//   maps:   ford   6x3: eirik (hero) lyra (ranger) + an unnamed hero on blue, the recruit "wanderer" (grunt) on
//                  red, two grunts; start events recruit wanderer (changeSide) and add "scout" (addUnit)
//           mill   6x3: eirik lyra wanderer scout on blue, "brute" on red; walking eirik to (3,1) sets bridge_cut
//           keep   6x3: eirik lyra wanderer scout on blue, one grunt on red
//           skirmish 4x2: a map for free battles
//   sides:  blue (human, team blue), red (ai)
#pragma once
#include <string>

#include "core/campaign/campaign_save.h"
#include "core/campaign/playthrough.h"
#include "core/data/loader.h"
#include "core/script/formulas.h"
#include "platform/platform.h"
#include "save_helpers.h"
#include "test.h"

namespace camptest {

using namespace ote;

inline const char* kGameJson = R"({
  "formatVersion": 1, "name": "Campaign Fixture",
  "stats": [ {"id":"hp"}, {"id":"atk"}, {"id":"def"}, {"id":"mov"} ],
  "roles": { "hp": "hp", "move": "mov" },
  "resources": [ {"id":"gold"} ],
  "moveTypes": [ {"id":"foot"} ],
  "switches": [ {"id":"met_wanderer","name":"Met"}, {"id":"bridge_cut","name":"Cut"} ],
  "variables": [ {"id":"kills","name":"Kills"}, {"id":"chapters","name":"Chapters"} ],
  "rules": { "defeat": ["noUnits"], "counterAttacks": true,
             "items": { "weapons": "fixed", "inventorySize": 3, "storage": "anywhere" },
             "progression": { "enabled": true, "xpPerLevel": 100, "maxLevel": 20, "xpHit": "30", "xpKill": "60" } },
  "formulas": { "damage": "a.atk - b.def" },
  "maps": [ {"id":"ford","name":"The Ford","file":"maps/ford.json"}, {"id":"mill","name":"The Mill","file":"maps/mill.json"},
            {"id":"keep","name":"The Keep","file":"maps/keep.json"}, {"id":"skirmish","name":"Skirmish","file":"maps/skirmish.json"} ]
})";

inline const char* kWeapons = R"({ "weapons": [
  { "id":"sword","name":"Sword","props":{"might":1} },
  { "id":"bow","name":"Bow","range":[2,2],"counter":false,"props":{"might":1} } ] })";
inline const char* kTerrain = R"({ "terrain": [ { "id":"plain","name":"Plain" } ] })";
inline const char* kItems = R"({ "items": [
  { "id":"potion","name":"Potion","kind":"usable","uses":3,"heal":5 },
  { "id":"tonic","name":"Tonic","kind":"usable","uses":1,"raise":{"atk":1} },
  { "id":"ring","name":"Ring","kind":"accessory","bonus":{"hp":4} } ] })";
inline const char* kUnits = R"({ "units": [
  { "id":"hero","name":"Hero","moveType":"foot","stats":{"hp":20,"atk":6,"def":3,"mov":4},"growths":{"hp":100,"atk":100},"weapons":["sword"] },
  { "id":"ranger","name":"Ranger","moveType":"foot","stats":{"hp":12,"atk":5,"def":1,"mov":4},"growths":{"hp":100},"weapons":["bow"] },
  { "id":"grunt","name":"Grunt","moveType":"foot","stats":{"hp":4,"atk":3,"def":1,"mov":3},"weapons":["sword"] },
  { "id":"brute","name":"Brute","moveType":"foot","stats":{"hp":30,"atk":12,"def":6,"mov":3},"weapons":["sword"] } ] })";

// A 6 x 3 field of plain ground with the given sides, units and events (JSON fragments).
inline std::string mapJson(int w, int h, const std::string& sides, const std::string& units, const std::string& events = "[]") {
  std::string row(w, '.'), tiles;
  for (int y = 0; y < h; ++y) tiles += std::string(y ? "," : "") + "\"" + row + "\"";
  return "{ \"name\":\"m\", \"grid\":{\"type\":\"square\",\"width\":" + std::to_string(w) + ",\"height\":" + std::to_string(h) +
         "}, \"legend\":{\".\":\"plain\"}, \"tiles\":[" + tiles + "], \"sides\":" + sides + ", \"units\":" + units +
         ", \"events\":" + events + " }";
}

inline std::string sides(const char* gold, const char* storage) {
  return std::string("[ {\"id\":\"blue\",\"controller\":\"human\",\"resources\":{\"gold\":") + gold + "},\"storage\":" + storage +
         "}, {\"id\":\"red\",\"controller\":\"ai\"} ]";
}

inline std::string fordMap() {
  return mapJson(
      6, 3, sides("100", "[\"potion\"]"),
      R"([ {"id":"eirik","type":"hero","side":"blue","x":0,"y":1,"name":"Eirik","items":["ring","potion"]},
           {"id":"lyra","type":"ranger","side":"blue","x":0,"y":0,"name":"Lyra"},
           {"type":"hero","side":"blue","x":0,"y":2,"name":"Extra"},
           {"id":"wanderer","type":"grunt","side":"red","x":5,"y":1,"name":"Wanderer"},
           {"type":"grunt","side":"red","x":5,"y":0},{"type":"grunt","side":"red","x":5,"y":2} ])",
      R"([ {"id":"recruit","when":{"battle":"start"},"do":[
             {"changeSide":"wanderer","side":"blue"},{"setSwitch":"met_wanderer","to":true},{"setVariable":"kills","to":2},
             {"addUnit":{"id":"scout","type":"ranger","side":"blue","x":1,"y":1,"name":"Scout","level":2}} ]} ])");
}

inline std::string heroesOnBlue(const char* extraUnits) {
  return std::string(R"([ {"id":"eirik","type":"hero","side":"blue","x":0,"y":1,"name":"Eirik","level":1},
           {"id":"lyra","type":"ranger","side":"blue","x":0,"y":0,"name":"Lyra","level":1},
           {"id":"wanderer","type":"grunt","side":"blue","x":0,"y":2,"name":"Wanderer"},
           {"id":"scout","type":"ranger","side":"blue","x":1,"y":2,"name":"Scout"},)") + extraUnits + "]";
}

inline std::string millMap() {
  return mapJson(6, 3, sides("50", "[\"tonic\"]"),
                 heroesOnBlue(R"({"id":"brute","type":"brute","side":"red","x":5,"y":1,"name":"Brute"})"),
                 R"([ {"id":"cut","when":{"reach":{"x":3,"y":1},"by":"eirik"},"do":[{"setSwitch":"bridge_cut","to":true}]} ])");
}
inline std::string keepMap() {
  return mapJson(6, 3, sides("0", "[]"), heroesOnBlue(R"({"type":"grunt","side":"red","x":5,"y":1})"));
}
inline std::string skirmishMap() {
  return mapJson(4, 2, sides("0", "[]"), R"([ {"type":"hero","side":"blue","x":0,"y":0}, {"type":"grunt","side":"red","x":3,"y":1} ])");
}

// The default campaign: ford -> mill -> (keep | keep) -> keep. See the tests for what each part checks.
inline const char* kCampaignJson = R"({
  "name": "The Ford War",
  "rules": { "fallen": "gone", "onDefeat": "retry" },
  "freeBattles": ["skirmish"],
  "chapters": [
    { "id": "ch1", "name": "The Ford", "map": "ford",
      "before": [ {"say":"We hold the ford.","speaker":"eirik"}, {"say":"Stay sharp.","speakerName":"Narrator","portrait":"narr"},
                  {"addVariable":"chapters","by":1} ],
      "after":  [ {"say":"It is done.","speaker":"lyra"}, {"giveResource":{"gold":25},"side":"blue"},
                  {"if":[{"alive":"wanderer"}],"then":[{"say":"Welcome, wanderer.","speaker":"eirik"}],"else":[{"say":"Alone again."}]} ] },
    { "id": "ch2", "name": "The Mill", "map": "mill",
      "before": [ {"addVariable":"chapters","by":1} ],
      "after":  [ {"if":[{"switch":"bridge_cut"}],"then":[{"goto":"ch3b"}]} ] },
    { "id": "ch3a", "name": "The Keep", "map": "keep", "next": "ch4", "before": [ {"say":"The keep, by the road."} ] },
    { "id": "ch3b", "name": "The Keep, by night", "map": "keep", "before": [ {"say":"The keep, by night."} ] },
    { "id": "ch4", "name": "Home", "map": "skirmish", "after": [ {"say":"The end."} ] }
  ]
})";

// The fixture game on disk. `campaign` = the text of data/campaign.json ("" = the game has none).
struct Game {
  Database db;
  Formulas formulas;
  std::string dir;
  std::string keepJson;  // replaces the keep map when set
  std::string researchJson;  // data/research.json when set (else the game has no research)

  static bool write(const std::string& path, const std::string& text) { return writeTextFile(path, text); }

  bool load(const std::string& folder, const std::string& campaign = kCampaignJson, std::string* errOut = nullptr) {
    dir = savetest::tempFolder(folder);
    makeDir(dir + "/data");
    makeDir(dir + "/maps");
    bool ok = write(dir + "/game.json", kGameJson) && write(dir + "/data/weapons.json", kWeapons) &&
              write(dir + "/data/terrain.json", kTerrain) && write(dir + "/data/units.json", kUnits) &&
              write(dir + "/data/items.json", kItems) && write(dir + "/maps/ford.json", fordMap()) &&
              write(dir + "/maps/mill.json", millMap()) && write(dir + "/maps/keep.json", keepJson.empty() ? keepMap() : keepJson) &&
              write(dir + "/maps/skirmish.json", skirmishMap());
    std::string researchPath = dir + "/data/research.json";
    if (researchJson.empty()) removeFile(researchPath);
    else ok = ok && write(researchPath, researchJson);
    std::string campPath = dir + "/data/campaign.json";
    if (campaign.empty()) removeFile(campPath);
    else ok = ok && write(campPath, campaign);
    if (!ok) return false;
    std::string err;
    ok = loadGame(dir, db, err);
    if (ok) ok = formulas.init(db, err);
    if (!ok && errOut) *errOut = err;
    else if (!ok) std::printf("  load: %s\n", err.c_str());
    return ok;
  }
  // Game load result only: the loader's message, "" = fine.
  std::string loadError(const std::string& folder, const std::string& campaign) {
    std::string err;
    return load(folder, campaign, &err) ? "" : err;
  }

  int unitId(const Battle& b, const char* id) const { return b.unitByStoryId(id); }
};

// Plays the battle with the AI on every side until it is decided (or `maxSteps`).
inline void playOut(Battle& b, std::vector<Event>& events, int maxRounds = 40) {
  while (!b.over && b.round <= maxRounds) {
    int guard = 0;
    while (aiTakeStep(b, events) && ++guard < 1000) {}  // true = the side still has its turn
  }
}

}  // namespace camptest
