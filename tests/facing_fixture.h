// Shared by the facing tests (docs/SRPG.md "Facing"): a scene of tests/fixtures/facing with a map written for the test.
//
// Numbers of the fixture: every fighter has str 6 / def 2 (the dummy str 3, hp 60), hp 40, damage = str + might - def + a bonus by the arc of the
// attack: +2 from the side, +5 from the back (the fixture's formula reads `arc`; the default formulas of a game do not). Sword 4 (so 8 from the
// front, 10 from the side, 13 from the back), bow [2, 3] 3, rifle [2, 6] in a straight line 5, bomb [2, 4] a burst 3. The arc "back" is called
// "Backstab" by the fixture (rules.facing.arcs).
#pragma once
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "srpg_skills_fixture.h"

namespace facingtest {

using namespace ote;
using skilltest::Copy;
using skilltest::World;

struct U {
  const char* type;
  const char* side;  // "blue" (the side to act first) or "red"
  int x, y;
  const char* id;
  const char* facing = "";  // "" = the default
};

// `rows` of plain squares (a '#' is a wall, 'w' woods).
inline std::string mapJson(const std::vector<std::string>& rows, const std::vector<U>& units, bool bothAi = false, const char* gridType = "square",
                           const std::vector<std::string>& heights = std::vector<std::string>()) {
  const int h = (int)rows.size(), w = (int)rows[0].size();
  std::string hs;  // optional "heights": rows of digits
  for (size_t y = 0; y < heights.size(); ++y) {
    hs += std::string(y ? ", [" : ",\n  \"heights\": [[");
    for (size_t x = 0; x < heights[y].size(); ++x) hs += std::string(x ? "," : "") + heights[y][x];
    hs += "]";
  }
  if (!heights.empty()) hs += "]";
  std::string t;
  for (int y = 0; y < h; ++y) t += std::string(y ? ", \"" : "\"") + rows[(size_t)y] + "\"";
  std::string out = std::string("{ \"name\": \"Test\", \"grid\": { \"type\": \"") + gridType + "\", \"width\": " + std::to_string(w) +
                    ", \"height\": " + std::to_string(h) + " }, \"legend\": { \".\": \"plain\", \"#\": \"wall\", \"w\": \"woods\" },\n  \"tiles\": [" + t +
                    "]" + hs + ",\n  \"sides\": [ { \"id\": \"blue\", \"controller\": \"" + (bothAi ? "ai" : "human") +
                    "\" }, { \"id\": \"red\", \"controller\": \"ai\" } ],\n  \"units\": [";
  for (size_t i = 0; i < units.size(); ++i) {
    const U& u = units[i];
    out += std::string(i ? ",\n    " : "\n    ") + "{ \"type\": \"" + u.type + "\", \"side\": \"" + u.side + "\", \"x\": " + std::to_string(u.x) +
           ", \"y\": " + std::to_string(u.y) + ", \"id\": \"" + u.id + "\"" + (i == 0 ? ", \"leader\": true" : "") +
           (u.facing[0] ? std::string(", \"facing\": \"") + u.facing + "\"" : std::string()) + " }";
  }
  return out + " ] }";
}

inline std::vector<std::string> plain(int w, int h) { return std::vector<std::string>((size_t)h, std::string((size_t)w, '.')); }

// The fixture game with facing cut out, and the "facing" keys of the units go with it (a game that never heard of facing).
inline void dropFacing(Copy& c) {
  c.swap("game.json", ",\n             \"facing\": { \"arcs\": { \"back\": { \"name\": \"Backstab\" } } }", "");
  c.swap("game.json", "a.str + w.might - b.def + (arc == 1 and 2 or 0) + (arc == 2 and 5 or 0)", "a.str + w.might - b.def");
  c.swap("data/units.json", ", \"skills\": [\"backstabber\"]", "");
  c.swap("data/units.json", ", \"skills\": [\"alert\"]", "");
  c.put("data/skills.json", "{ \"skills\": [] }");
}

struct Scene {
  Copy c;
  std::unique_ptr<World> w;
  Scene(const std::string& name, const std::vector<std::string>& rows, const std::vector<U>& units, bool bothAi = false,
        const std::function<void(Copy&)>& tweak = nullptr, const char* gridType = "square", const std::vector<std::string>& heights = std::vector<std::string>())
      : c(name, "facing") {
    if (std::string(gridType) == "hex") c.swap("game.json", "\"formatVersion\": 1,", "\"formatVersion\": 1, \"grid\": { \"type\": \"hex\", \"layout\": \"pointy\" },");
    c.put("maps/field.json", mapJson(rows, units, bothAi, gridType, heights));
    if (tweak) tweak(c);
    w.reset(new World("field", c.dir));
  }
  Battle& b() { return w->b(); }
  const Database& db() { return w->db(); }
  Cell at(int x, int y) { return b().grid.at(x, y); }
  int weapon(const char* id) { return db().findWeapon(id); }
  int id(const char* story) { return w->id(story); }
  Unit& unit(const char* story) { return w->unit(story); }
  bool go(const char* who, int x, int y, std::string* why = nullptr) { return w->apply(Action::move(id(who), at(x, y)), why); }
  bool hit(const char* who, const char* target, const char* weaponId, std::string* why = nullptr) {
    return w->apply(Action::attack(id(who), id(target), weapon(weaponId)), why);
  }
  bool wait(const char* who, int facing = -1, std::string* why = nullptr) {
    Action a = Action::wait(id(who));
    a.facing = facing;
    return w->apply(a, why);
  }
  std::string refused(const Action& a) {
    std::string why;
    return b().validate(a, &why) ? std::string("(allowed)") : why;
  }
  // The forecast of `who` attacking `target` from where it stands.
  Forecast forecast(const char* who, const char* target, const char* weaponId) {
    Forecast f;
    b().forecast(id(who), id(target), weapon(weaponId), unit(who).cell, f);
    return f;
  }
  int count(EventType t) { return w->count(t); }
  // The UnitFaced events since the last action as "unit>dir" words.
  std::string faced() {
    std::string out;
    for (const Event& e : w->events)
      if (e.type == EventType::UnitFaced) out += (out.empty() ? "" : " ") + std::to_string(e.a) + ">" + std::to_string(e.b);
    return out;
  }
};

}  // namespace facingtest
