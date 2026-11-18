// Helpers of the scripted-style computer player tests (docs/AI.md "Scripted-style computer players"): a tiny generated game and a map the test
// writes itself. Damage is `a.str + w.might - b.def` and always hits, so every number is exact (hit chance is a parameter).
//
//   soldier: hp 20 str 6 def 2 mov 4, sword (range 1, might 5).        archer: hp 16 str 5 def 1 mov 4, bow (range 2 only, might 4).
//   twin:    a soldier with two swords (sword, sword2: the same numbers), to tell weapons apart.
//   Terrain: plain ".", forest "f" (foot cost 2, defense 1, avoid 10), fort "F" (defense 2, avoid 20). Item: potion (usable, heals 10, 2 uses).
//   Sides: blue (a person, passive: the tests end its turn) and red (the computer). Map "field": 14 x 5, a throne structure at 12,2.
#pragma once
#include <string>
#include <vector>

#include "core/ai/ai.h"
#include "core/ai/ai_internal.h"
#include "core/data/loader.h"
#include "platform/platform.h"
#include "save_helpers.h"

namespace scripted {

using namespace ote;

inline std::string gameJson(const std::string& aiRules, const std::string& hit) {
  return std::string(R"({ "formatVersion": 1, "name": "Scripted AI fixture", "view": { "width": 480, "height": 270, "tileSize": 24 },
  "stats": [ { "id": "hp", "name": "HP" }, { "id": "str", "name": "Strength" }, { "id": "def", "name": "Defense" }, { "id": "mov", "name": "Move" } ],
  "roles": { "hp": "hp", "move": "mov" },
  "moveTypes": [ { "id": "foot", "name": "Foot" } ],
  "rules": { "defeat": ["noUnits"], "counterAttacks": true, "items": { "weapons": "fixed", "inventorySize": 4 })") +
         (aiRules.empty() ? "" : ", \"ai\": " + aiRules) + R"( },
  "formulas": { "damage": "a.str + w.might - b.def", "hit": ")" + hit + R"(" },
  "maps": [ { "id": "field", "name": "Field", "file": "maps/field.json" } ] })";
}

inline const char* terrainJson() {
  return R"({ "terrain": [ { "id": "plain", "name": "Plain", "props": { "defense": 0, "avoid": 0 } },
    { "id": "forest", "name": "Forest", "moveCost": { "foot": 2 }, "props": { "defense": 1, "avoid": 10 } },
    { "id": "fort", "name": "Fort", "props": { "defense": 2, "avoid": 20 } } ] })";
}
inline const char* weaponsJson() {
  return R"({ "weapons": [ { "id": "sword", "name": "Sword", "group": "sword", "props": { "might": 5 } },
                          { "id": "bow", "name": "Bow", "group": "bow", "range": [2, 2], "props": { "might": 4 } },
                          { "id": "sword2", "name": "Sword 2", "group": "sword", "props": { "might": 5 } } ] })";
}
inline const char* unitsJson() {
  return R"({ "units": [
    { "id": "soldier", "name": "Soldier", "glyph": "S", "moveType": "foot", "stats": { "hp": 20, "str": 6, "def": 2, "mov": 4 }, "weapons": ["sword"] },
    { "id": "archer", "name": "Archer", "glyph": "A", "moveType": "foot", "stats": { "hp": 16, "str": 5, "def": 1, "mov": 4 }, "weapons": ["bow"] },
    { "id": "twin", "name": "Twin", "glyph": "T", "moveType": "foot", "stats": { "hp": 20, "str": 6, "def": 2, "mov": 4 }, "weapons": ["sword", "sword2"] } ] })";
}
inline const char* itemsJson() { return R"({ "items": [ { "id": "potion", "name": "Potion", "kind": "usable", "uses": 2, "heal": 10 } ] })"; }
inline const char* structuresJson() { return R"({ "structures": [ { "id": "throne", "name": "Throne", "color": "#ccaa44", "glyph": "T" } ] })"; }

// `rows`: the five rows of the map, each 14 characters (".", "f", "F"); empty = all plain.
inline std::string mapJson(const std::string& units, const std::string& rows, const std::string& events) {
  std::vector<std::string> r(5, "..............");
  if (!rows.empty()) {
    size_t from = 0;
    for (int i = 0; i < 5; ++i) {
      const size_t to = rows.find('/', from);
      r[(size_t)i] = rows.substr(from, to == std::string::npos ? std::string::npos : to - from);
      from = to == std::string::npos ? rows.size() : to + 1;
    }
  }
  std::string tiles;
  for (int y = 0; y < 5; ++y) tiles += std::string(y ? "," : "") + "\"" + r[(size_t)y] + "\"";
  return std::string(R"({ "name": "Field", "grid": { "type": "square", "width": 14, "height": 5 },
  "legend": { ".": "plain", "f": "forest", "F": "fort" }, "tiles": [ )") + tiles + R"( ],
  "sides": [ { "id": "blue", "controller": "human" }, { "id": "red", "controller": "ai" } ],
  "structures": [ { "type": "throne", "x": 12, "y": 2, "id": "throne" } ],
  "units": [ )" + units + " ]" + (events.empty() ? "" : ", \"events\": [ " + events + " ]") + " }";
}

inline std::string red(const char* id, int x, int y, const std::string& extra = "", const char* type = "soldier") {
  return std::string("{ \"type\": \"") + type + "\", \"id\": \"" + id + "\", \"side\": \"red\", \"x\": " + std::to_string(x) + ", \"y\": " + std::to_string(y) +
         (extra.empty() ? "" : ", " + extra) + " }";
}
inline std::string blue(const char* id, int x, int y, const char* type = "soldier") {
  return std::string("{ \"type\": \"") + type + "\", \"id\": \"" + id + "\", \"side\": \"blue\", \"x\": " + std::to_string(x) + ", \"y\": " + std::to_string(y) + " }";
}
inline std::string beh(const std::string& body) { return "\"behaviour\": " + body; }
inline bool says(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

struct Options {
  std::string aiRules;  // the text of "rules.ai" ("" = none)
  std::string rows;     // see mapJson
  std::string events;
  std::string hit = "100";
};

// The game and map on disk, loaded, battle started. `units` are the entries of the map's units after the blue ones the test names.
struct SField {
  std::string dir;
  savetest::Rig rig;
  std::vector<Event> events;
  bool loaded = false;

  static std::string write(const std::string& name, const std::string& units, const Options& o) {
    const std::string dir = savetest::tempFolder(name);
    makeDir(dir + "/data");
    makeDir(dir + "/maps");
    writeTextFile(dir + "/game.json", gameJson(o.aiRules, o.hit));
    writeTextFile(dir + "/data/terrain.json", terrainJson());
    writeTextFile(dir + "/data/weapons.json", weaponsJson());
    writeTextFile(dir + "/data/units.json", unitsJson());
    writeTextFile(dir + "/data/items.json", itemsJson());
    writeTextFile(dir + "/data/structures.json", structuresJson());
    writeTextFile(dir + "/maps/field.json", mapJson(units, o.rows, o.events));
    return dir;
  }
  // `units`: every unit of the map, written by the test (the blue "lord" is NOT added for you).
  SField(const std::string& name, const std::string& units, const Options& o = Options()) {
    dir = write(name, units, o);
    loaded = rig.load(dir, "field");
    if (loaded) rig.battle.start(events);
  }
  // The text the loader says about the game and map ("" = both load).
  static std::string error(const std::string& name, const std::string& units, const Options& o = Options()) {
    const std::string dir = write(name, units, o);
    Database db;
    MapDef m;
    std::string err;
    return loadGame(dir, db, err) && loadMap(db, "field", m, err) ? "" : err;
  }

  Battle& b() { return rig.battle; }
  int id(const char* story) { return b().unitByStoryId(story); }
  Unit& unit(const char* story) { return b().units[(size_t)id(story)]; }
  Cell at(int x, int y) { return b().grid.at(x, y); }
  int x(const char* story) { return b().grid.x(unit(story).cell); }
  int y(const char* story) { return b().grid.y(unit(story).cell); }
  int hp(const char* who) { return unit(who).hp; }
  // Blue ends its turn, then the computer plays red's whole turn. False when red did not hand the turn back.
  bool redTurn() {
    events.clear();
    if (!b().apply(Action::endTurn(), events)) return false;
    for (int i = 0; i < 80 && !b().over && b().activeSide == 1; ++i) aiTakeStep(b(), events);
    return b().over || b().activeSide == 0;
  }
  // The computer's steps of red's turn one at a time: the story ids of the units that finished, in that order. Blue ends its turn first.
  std::vector<std::string> redSequence() {
    std::vector<std::string> seq;
    events.clear();
    if (!b().apply(Action::endTurn(), events)) return seq;
    std::vector<char> was(b().units.size(), 0);
    for (const Unit& u : b().units) was[(size_t)u.id] = u.done;
    for (int i = 0; i < 80 && !b().over && b().activeSide == 1; ++i) {
      aiTakeStep(b(), events);
      for (const Unit& u : b().units)
        if (u.side == 1 && u.done && !was[(size_t)u.id]) {
          was[(size_t)u.id] = 1;
          seq.push_back(u.storyId);
        }
    }
    return seq;
  }
};

}  // namespace scripted
