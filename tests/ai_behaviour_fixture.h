// Helpers of the computer-opponent behaviour tests (docs/AI.md): a tiny generated game and a map the test writes itself, so each
// test states its units in one line. Plain terrain, no items: every unit type carries its own weapon, damage is
// `a.str + w.might - b.def` and always hits, so every outcome is exact.
//
//   soldier: hp 20 str 6 def 2 mov 4, sword (range 1, might 5).   archer: hp 16 str 5 def 1 mov 4, bow (range 2, might 4).
//   Sides: blue (a person, passive in these tests: they end its turn) and red (the computer, the side under test).
//   Map "field": 14 x 5 (hex or square), a throne structure "throne" at 12,2.
#pragma once
#include <string>

#include "core/ai/ai.h"
#include "core/data/loader.h"
#include "platform/platform.h"
#include "save_helpers.h"

namespace aitest {

using namespace ote;

inline std::string gameJson(bool hex) {
  return std::string(R"({ "formatVersion": 1, "name": "AI behaviour fixture", "view": { "width": 480, "height": 270, "tileSize": 24 },)") +
         (hex ? R"( "grid": { "type": "hex", "layout": "pointy" },)" : "") + R"(
  "stats": [ { "id": "hp", "name": "HP" }, { "id": "str", "name": "Strength" }, { "id": "def", "name": "Defense" }, { "id": "mov", "name": "Move" } ],
  "roles": { "hp": "hp", "move": "mov" },
  "moveTypes": [ { "id": "foot", "name": "Foot" } ],
  "switches": [ { "id": "escaped", "name": "Escaped" }, { "id": "woken", "name": "Woken" } ],
  "variables": [ { "id": "out", "name": "Out" } ],
  "rules": { "defeat": ["noUnits"], "counterAttacks": true },
  "formulas": { "damage": "a.str + w.might - b.def", "hit": "100" },
  "maps": [ { "id": "field", "name": "Field", "file": "maps/field.json" } ] })";
}

inline const char* terrainJson() { return R"({ "terrain": [ { "id": "plain", "name": "Plain" } ] })"; }
inline const char* weaponsJson() {
  return R"({ "weapons": [ { "id": "sword", "name": "Sword", "group": "sword", "props": { "might": 5 } },
                          { "id": "bow", "name": "Bow", "group": "bow", "range": [2, 2], "props": { "might": 4 } } ] })";
}
inline const char* unitsJson() {
  return R"({ "units": [
    { "id": "soldier", "name": "Soldier", "glyph": "S", "moveType": "foot", "stats": { "hp": 20, "str": 6, "def": 2, "mov": 4 }, "weapons": ["sword"] },
    { "id": "archer", "name": "Archer", "glyph": "A", "moveType": "foot", "stats": { "hp": 16, "str": 5, "def": 1, "mov": 4 }, "weapons": ["bow"] } ] })";
}
inline const char* structuresJson() { return R"({ "structures": [ { "id": "throne", "name": "Throne", "color": "#ccaa44", "glyph": "T" } ] })"; }
inline const char* conditionsJson() {
  return R"({ "conditions": [ { "id": "berserk", "name": "Berserk", "tag": "Br", "turns": 3, "blocks": [ { "when": "always", "do": [ { "berserk": true } ] } ] } ] })";
}

// The map's text. `units`: the entries of "units" (comma separated, written by the test); `events`: the entries of "events".
inline std::string mapJson(bool hex, const std::string& units, const std::string& events = "") {
  std::string tiles;
  for (int y = 0; y < 5; ++y) tiles += std::string(y ? "," : "") + "\"..............\"";
  return std::string(R"({ "name": "Field", "grid": { "type": ")") + (hex ? "hex" : "square") + R"(", "width": 14, "height": 5 },
  "legend": { ".": "plain" }, "tiles": [ )" + tiles + R"( ],
  "sides": [ { "id": "blue", "controller": "human" }, { "id": "red", "controller": "ai" } ],
  "structures": [ { "type": "throne", "x": 12, "y": 2, "id": "throne" } ],
  "units": [ )" + units + " ]" + (events.empty() ? "" : ", \"events\": [ " + events + " ]") + " }";
}

// The game and map on disk, loaded, battle started.
struct Field {
  std::string dir;
  savetest::Rig rig;
  std::vector<Event> events;
  bool loaded = false;

  static std::string write(const std::string& name, const std::string& units, const std::string& events, bool hex) {
    const std::string dir = savetest::tempFolder(name);
    makeDir(dir + "/data");
    makeDir(dir + "/maps");
    writeTextFile(dir + "/game.json", gameJson(hex));
    writeTextFile(dir + "/data/terrain.json", terrainJson());
    writeTextFile(dir + "/data/weapons.json", weaponsJson());
    writeTextFile(dir + "/data/units.json", unitsJson());
    writeTextFile(dir + "/data/structures.json", structuresJson());
    writeTextFile(dir + "/data/conditions.json", conditionsJson());
    writeTextFile(dir + "/maps/field.json", mapJson(hex, units, events));
    return dir;
  }
  // `units` always starts with the blue lord (id "lord", at 0,2 unless `lordAt` says otherwise).
  Field(const std::string& name, const std::string& redUnits, const std::string& events = "", bool hex = false, const char* lordAt = "\"x\": 0, \"y\": 2") {
    dir = write(name, std::string(R"({ "type": "soldier", "id": "lord", "side": "blue", )") + lordAt + " }" +
                          (redUnits.empty() ? "" : ", " + redUnits), events, hex);
    loaded = rig.load(dir, "field");
    if (loaded) rig.battle.start(this->events);
  }
  // The text the loader says about the map ("" = it loaded); a standalone helper for the refusal tests.
  static std::string mapError(const std::string& name, const std::string& redUnits, const std::string& events = "", bool hex = false) {
    const std::string dir = write(name, std::string(R"({ "type": "soldier", "id": "lord", "side": "blue", "x": 0, "y": 2 }, )") + redUnits, events, hex);
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
  bool sw(const char* name) { return b().storyState.switches[(size_t)rig.db.findSwitch(name)] != 0; }
  void put(const char* who, int px, int py) {
    Unit& u = unit(who);
    if (u.cell != kNoCell) b().occupant[(size_t)u.cell] = -1;
    u.cell = at(px, py);
    b().occupant[(size_t)u.cell] = u.id;
    b().refreshSight();
  }
  // Blue ends its turn, then the computer plays red's whole turn. False when red did not hand the turn back.
  bool redTurn() {
    events.clear();
    if (!b().apply(Action::endTurn(), events)) return false;
    for (int i = 0; i < 80 && !b().over && b().activeSide == 1; ++i) aiTakeStep(b(), events);
    return b().over || b().activeSide == 0;
  }
  int hp(const char* who) { return unit(who).hp; }
};

}  // namespace aitest
