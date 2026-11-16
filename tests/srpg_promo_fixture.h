// A small game for the promotion tests (docs/SRPG.md step P1), written into a temp folder so a test can change one
// file and load the result. Built on charstest::Copy (the temp-folder helper of the characters tests): the files of
// tests/fixtures/srpg-chars it copies are all overwritten here.
//
// Unit types: recruit (promotes to captain or scout by the writ or by an offer at level 3), captain, scout (cannot
// wield blades), cadet (becomes a captain by itself at level 2), pawn (offer to rider), drone (auto to rider),
// rider (a "wheel" mover: cannot stand on rock), grunt (the enemy). The map's units are all on the blue side except
// the grunts; ids: rec (level 3, a writ and a sword), rec2 (level 1), cad, pwn (on rock), drn (on rock).
#pragma once
#include <string>

#include "srpg_chars_fixture.h"

namespace promotest {

using namespace ote;
using charstest::has;

inline const char* kGame = R"({
  "formatVersion": 1, "name": "Promotion Fixture",
  "stats": [ {"id":"hp"}, {"id":"atk"}, {"id":"def"}, {"id":"mov"} ],
  "roles": { "hp": "hp", "move": "mov" },
  "moveTypes": [ {"id":"foot"}, {"id":"wheel"} ],
  "rules": { "defeat": ["noUnits"], "counterAttacks": true,
             "items": { "weapons": "carried", "inventorySize": 4 },
             "progression": { "enabled": true, "xpPerLevel": 100, "maxLevel": 20, "xpHit": "30", "xpKill": "60" } },
  "formulas": { "damage": "a.atk - b.def" },
  "maps": [ {"id":"field","name":"Field","file":"maps/field.json"} ]
})";

inline const char* kTerrain = R"({ "terrain": [ { "id":"plain","name":"Plain" },
  { "id":"rock","name":"Rock","moveCost":{"wheel":0} } ] })";

inline const char* kWeapons = R"({ "weapons": [ { "id":"sword","name":"Sword","group":"blade","props":{"might":1} },
  { "id":"bow","name":"Bow","group":"bow","props":{"might":1} } ] })";

inline const char* kItems = R"({ "items": [
  { "id":"writ","name":"Writ","kind":"usable","uses":1,"promotes":true },
  { "id":"elixir","name":"Elixir","kind":"usable","uses":1,"heal":5,"promotes":true },
  { "id":"potion","name":"Potion","kind":"usable","uses":1,"heal":5 },
  { "id":"blade","name":"Blade","kind":"weapon","weapon":"sword" },
  { "id":"longbow","name":"Longbow","kind":"weapon","weapon":"bow" } ] })";

inline const char* kUnits = R"({ "units": [
  { "id":"recruit","name":"Recruit","moveType":"foot","stats":{"hp":10,"atk":4,"def":2,"mov":4},"growths":{"hp":100},
    "wields":["blade"],"items":["blade","writ","longbow"],
    "promotions":[ {"to":"captain","fromLevel":3,"items":["writ","elixir"],"atLevel":"offer"},
                   {"to":"scout","fromLevel":3,"items":["writ"],"levelAfter":"keep","gains":{"hp":3,"mov":1}} ] },
  { "id":"captain","name":"Captain","moveType":"foot","stats":{"hp":16,"atk":7,"def":4,"mov":4},"wields":["blade"] },
  { "id":"scout","name":"Scout","moveType":"foot","stats":{"hp":12,"atk":5,"def":2,"mov":6},"wields":["bow"] },
  { "id":"cadet","name":"Cadet","moveType":"foot","stats":{"hp":8,"atk":3,"def":1,"mov":3},"growths":{"atk":0},
    "wields":["blade"],"items":["blade"],
    "promotions":[ {"to":"captain","fromLevel":2,"atLevel":"auto"} ] },
  { "id":"pawn","name":"Pawn","moveType":"foot","stats":{"hp":9,"atk":3,"def":1,"mov":3},"wields":["blade"],
    "promotions":[ {"to":"rider","atLevel":"offer","items":["writ"]} ] },
  { "id":"drone","name":"Drone","moveType":"foot","stats":{"hp":9,"atk":3,"def":1,"mov":3},"wields":["blade"],
    "promotions":[ {"to":"rider","fromLevel":2,"atLevel":"auto"} ] },
  { "id":"rider","name":"Rider","moveType":"wheel","stats":{"hp":14,"atk":6,"def":3,"mov":5},"wields":["blade"] },
  { "id":"grunt","name":"Grunt","moveType":"foot","stats":{"hp":8,"atk":3,"def":1,"mov":3},"wields":["blade"],"items":["blade"] } ] })";

// The map: row 0 has a rock square at x = 3 and x = 5. `events` is the text of the map's "events" list ("" = none).
inline std::string map(const std::string& events = "") {
  return R"({ "name": "Field", "grid": { "type": "square", "width": 8, "height": 3 }, "legend": { ".": "plain", "r": "rock" },
  "tiles": ["...r.r..", "........", "........"],
  "sides": [ { "id": "blue", "controller": "human" }, { "id": "red", "controller": "ai" } ],
  "units": [
    { "id": "rec",  "type": "recruit", "side": "blue", "x": 0, "y": 0, "level": 3, "leader": true },
    { "id": "rec2", "type": "recruit", "side": "blue", "x": 0, "y": 1 },
    { "id": "cad",  "type": "cadet",   "side": "blue", "x": 1, "y": 1 },
    { "id": "pwn",  "type": "pawn",    "side": "blue", "x": 3, "y": 0, "level": 1 },
    { "id": "drn",  "type": "drone",   "side": "blue", "x": 5, "y": 0, "level": 1 },
    { "type": "grunt", "side": "red", "x": 2, "y": 1 },
    { "type": "grunt", "side": "red", "x": 7, "y": 2 } ])" +
         (events.empty() ? std::string() : ", \"events\": [" + events + "]") + " }";
}

// A writable copy of the fixture game; `c.put` / `c.swap` change it before it is loaded.
struct Game : charstest::Copy {
  explicit Game(const std::string& name) : charstest::Copy(name) {
    drop("data/characters.json");
    drop("maps/plain.json");
    put("game.json", kGame);
    put("data/terrain.json", kTerrain);
    put("data/weapons.json", kWeapons);
    put("data/items.json", kItems);
    put("data/units.json", kUnits);
    put("maps/field.json", map());
  }
};

}  // namespace promotest
