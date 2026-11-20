// Shared by the height-rule tests (docs/SRPG.md "Height rules"): a scene of tests/fixtures/height-rules with a map written for the test, so every
// test lays its heights and units out where it needs them.
//
// Numbers of the fixture: jump 1 up and down by default (jump 3 up / 1 down for the goat, 0 for the stiff, 5 down for the diver, no limit for the
// hawk, whose move type "air" is a flier); every fighter has str 6 / def 2 (the dummy str 3, hp 60), hp 40, damage = str + might - def (sword 8).
// Weapons: sword (range 1), bow [2, 3], sling [1, 2] that cannot fire at anything higher than the shooter, pike (range 1) that reaches one level up
// or down, rifle [2, 6] in a straight line (a ridge higher than both ends stops it), bomb [2, 4] a burst whose victims must be within 1 level of the
// aimed square. rangeBonus: +1 maximum range per 2 levels above the target, at most +2, for weapons that reach more than one square.
#pragma once
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "srpg_skills_fixture.h"

namespace heighttest {

using namespace ote;
using skilltest::Copy;
using skilltest::World;

struct U {
  const char* type;
  const char* side;  // "blue" (the side to act first) or "red"
  int x, y;
  const char* id;
};

// Heights written as rows of digits ('0'..'9'); every square is plain unless `tiles` says otherwise ('#' wall, 'w' woods).
inline std::string mapJson(const std::vector<std::string>& heights, const std::vector<std::string>& tiles, const std::vector<U>& units,
                           bool bothAi = false, const char* gridType = "square") {
  const int h = (int)heights.size(), w = (int)heights[0].size();
  std::string t, hs;
  for (int y = 0; y < h; ++y) {
    t += std::string(y ? ", \"" : "\"") + (tiles.empty() ? std::string((size_t)w, '.') : tiles[(size_t)y]) + "\"";
    hs += std::string(y ? ", [" : "[");
    for (int x = 0; x < w; ++x) hs += std::string(x ? "," : "") + heights[(size_t)y][(size_t)x];
    hs += "]";
  }
  std::string out = std::string("{ \"name\": \"Test\", \"grid\": { \"type\": \"") + gridType + "\", \"width\": " + std::to_string(w) +
                    ", \"height\": " + std::to_string(h) + " }, \"legend\": { \".\": \"plain\", \"#\": \"wall\", \"w\": \"woods\" },\n  \"tiles\": [" + t +
                    "],\n  \"heights\": [" + hs + "],\n  \"sides\": [ { \"id\": \"blue\", \"controller\": \"" + (bothAi ? "ai" : "human") +
                    "\" }, { \"id\": \"red\", \"controller\": \"ai\" } ],\n  \"units\": [";
  for (size_t i = 0; i < units.size(); ++i) {
    const U& u = units[i];
    out += std::string(i ? ",\n    " : "\n    ") + "{ \"type\": \"" + u.type + "\", \"side\": \"" + u.side + "\", \"x\": " + std::to_string(u.x) +
           ", \"y\": " + std::to_string(u.y) + ", \"id\": \"" + u.id + "\"" + (i == 0 ? ", \"leader\": true" : "") + " }";
  }
  return out + " ] }";
}

// `n` rows of `w` zeros.
inline std::vector<std::string> flat(int w, int n) { return std::vector<std::string>((size_t)n, std::string((size_t)w, '0')); }

// The fixture game with the height rules cut out, and the keys only a height game accepts (a game that never heard of them).
inline void dropHeights(Copy& c) {
  c.swap("game.json", ",\n             \"height\": { \"jump\": { \"default\": 1 }, \"flyingMoveTypes\": [\"air\"],\n                         \"rangeBonus\": { \"perLevels\": 2, \"max\": 2 }, \"lineOfSight\": true }", "");
  c.swap("data/weapons.json", ", \"heightRange\": { \"up\": 0 }", "");
  c.swap("data/weapons.json", ", \"heightRange\": { \"up\": 1, \"down\": 1 }", "");
  c.swap("data/weapons.json", ", \"vertical\": 1", "");
  c.swap("data/units.json", ", \"jump\": { \"up\": 3, \"down\": 1 }", "");
  c.swap("data/units.json", ", \"jump\": 0", "");
  c.swap("data/units.json", ", \"jump\": { \"down\": 5 }", "");
}

struct Scene {
  Copy c;
  std::unique_ptr<World> w;
  Scene(const std::string& name, const std::vector<std::string>& heights, const std::vector<U>& units, bool bothAi = false,
        const std::function<void(Copy&)>& tweak = nullptr, const std::vector<std::string>& tiles = std::vector<std::string>(),
        const char* gridType = "square")
      : c(name, "height-rules") {
    if (std::string(gridType) == "hex") c.swap("game.json", "\"formatVersion\": 1,", "\"formatVersion\": 1, \"grid\": { \"type\": \"hex\", \"layout\": \"pointy\" },");
    c.put("maps/field.json", mapJson(heights, tiles, units, bothAi, gridType));
    if (tweak) tweak(c);
    w.reset(new World("field", c.dir));
  }
  Battle& b() { return w->b(); }
  const Database& db() { return w->db(); }
  Cell at(int x, int y) { return b().grid.at(x, y); }
  int weapon(const char* id) { return db().findWeapon(id); }
  int id(const char* story) { return w->id(story); }
  Unit& unit(const char* story) { return w->unit(story); }
  // Can `who` end a move on (x, y) right now?
  bool canStop(const char* who, int x, int y) {
    Reach r;
    b().reachable(id(who), r);
    return r.canStop(at(x, y));
  }
  bool go(const char* who, int x, int y, std::string* why = nullptr) { return w->apply(Action::move(id(who), at(x, y)), why); }
  bool hit(const char* who, const char* target, const char* weaponId, std::string* why = nullptr) {
    return w->apply(Action::attack(id(who), id(target), weapon(weaponId)), why);
  }
  std::string refused(const Action& a) {
    std::string why;
    return b().validate(a, &why) ? std::string("(allowed)") : why;
  }
  std::string refusedHit(const char* who, const char* target, const char* weaponId) {
    return refused(Action::attack(id(who), id(target), weapon(weaponId)));
  }
  std::string refusedMove(const char* who, int x, int y) { return refused(Action::move(id(who), at(x, y))); }
  // The targets `who` would be offered with `weaponId` from where it stands, as unit ids joined with spaces.
  std::string targets(const char* who, const char* weaponId) {
    std::vector<AttackOption> opts;
    b().attackOptions(id(who), unit(who).cell, opts);
    std::string out;
    for (const AttackOption& o : opts)
      if (o.weapon == weapon(weaponId)) out += (out.empty() ? "" : " ") + std::to_string(o.target);
    return out;
  }
  // The squares `who` could end a move on, as "x,y" words in ascending cell order.
  std::string stops(const char* who) {
    Reach r;
    b().reachable(id(who), r);
    std::vector<Cell> cells = r.stops;
    std::sort(cells.begin(), cells.end());
    std::string out;
    for (Cell c2 : cells) out += (out.empty() ? "" : " ") + std::to_string(b().grid.x(c2)) + "," + std::to_string(b().grid.y(c2));
    return out;
  }
};

}  // namespace heighttest
