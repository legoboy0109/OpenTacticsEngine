// Shared by the area-attack tests (docs/SRPG.md "Area attacks and line fire"): a scene of tests/fixtures/area-fire with a map
// written for the test, so every test lays its units out where it needs them.
//
// Numbers of the fixture: every fighter has str 6 / def 2, damage = str + might - def (sword 8, rifle 9, bomb 7, mortar 8, beam 8,
// cross 7, flak 7, rail 8, shell 7, minus 2 more in woods). Hit is 100 except the shell (70). dummy hp 12, frail hp 5, tough hp 60,
// every other type 30.
#pragma once
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "srpg_skills_fixture.h"

namespace areatest {

using namespace ote;
using skilltest::Copy;
using skilltest::World;

struct U {
  const char* type;
  const char* side;  // "blue" (the side to act first) or "red"
  int x, y;
  const char* id;
};

inline std::string mapJson(int w, int h, const std::vector<std::string>& rows, const std::vector<U>& units, bool bothAi = false,
                           const char* gridType = "square") {
  std::string t;
  for (size_t i = 0; i < rows.size(); ++i) t += (i ? ", \"" : "\"") + rows[i] + "\"";
  std::string out = std::string("{ \"name\": \"Test\", \"grid\": { \"type\": \"") + gridType + "\", \"width\": " + std::to_string(w) +
                    ", \"height\": " + std::to_string(h) + " }, \"legend\": { \".\": \"plain\", \"#\": \"wall\", \"w\": \"woods\" },\n  \"tiles\": [" + t +
                    "],\n  \"sides\": [ { \"id\": \"blue\", \"controller\": \"" + (bothAi ? "ai" : "human") +
                    "\" }, { \"id\": \"red\", \"controller\": \"ai\" } ],\n  \"units\": [";
  for (size_t i = 0; i < units.size(); ++i) {
    const U& u = units[i];
    out += std::string(i ? ",\n    " : "\n    ") + "{ \"type\": \"" + u.type + "\", \"side\": \"" + u.side + "\", \"x\": " + std::to_string(u.x) +
           ", \"y\": " + std::to_string(u.y) + ", \"id\": \"" + u.id + "\"" + (i == 0 ? ", \"leader\": true" : "") + " }";
  }
  return out + " ] }";
}

// An open plain map `w` x `h`; `walls` = rows of `#` and `.`.
inline std::vector<std::string> plain(int w, int h) { return std::vector<std::string>((size_t)h, std::string((size_t)w, '.')); }

struct Scene {
  Copy c;
  std::unique_ptr<World> w;
  Scene(const std::string& name, const std::vector<std::string>& rows, const std::vector<U>& units, bool bothAi = false,
        const std::function<void(Copy&)>& tweak = nullptr)
      : c(name, "area-fire") {
    c.put("maps/field.json", mapJson((int)rows[0].size(), (int)rows.size(), rows, units, bothAi));
    if (tweak) tweak(c);
    w.reset(new World("field", c.dir));
  }
  Battle& b() { return w->b(); }
  const Database& db() { return w->db(); }
  Cell at(int x, int y) { return b().grid.at(x, y); }
  int weapon(const char* id) { return db().findWeapon(id); }
  int id(const char* story) { return w->id(story); }
  Unit& unit(const char* story) { return w->unit(story); }
  // The attack of `who` with `weapon`, aimed at the square (x, y).
  Action shot(const char* who, const char* weaponId, int x, int y) {
    Action a = Action::attack(id(who), -1, weapon(weaponId));
    a.cell = at(x, y);
    const int there = b().unitAt(a.cell);
    a.target = there;
    return a;
  }
  bool fire(const char* who, const char* weaponId, int x, int y, std::string* why = nullptr) { return w->apply(shot(who, weaponId, x, y), why); }
  // The squares of an option list for `who` standing where it stands, as "x,y" words in ascending cell order.
  std::string aimable(const char* who, const char* weaponId) {
    std::vector<AttackOption> opts;
    b().attackOptions(id(who), unit(who).cell, opts);
    std::string out;
    for (const AttackOption& o : opts)
      if (o.weapon == weapon(weaponId)) out += (out.empty() ? "" : " ") + std::to_string(b().grid.x(o.cell)) + "," + std::to_string(b().grid.y(o.cell));
    return out;
  }
  // The event stream as text: "A3>5" an attack (a > b, "c" for a counter, "k" for a kill), "D5" a death.
  std::string story() {
    std::string out;
    for (const Event& e : w->events) {
      if (e.type == EventType::Attack)
        out += (out.empty() ? "" : " ") + std::string("A") + std::to_string(e.a) + ">" + std::to_string(e.b) + ((e.flags & kCounter) ? "c" : "") +
               ((e.flags & kKill) ? "k" : "");
      else if (e.type == EventType::UnitDied) out += (out.empty() ? "" : " ") + std::string("D") + std::to_string(e.a);
    }
    return out;
  }
  int attackEvents() {
    int n = 0;
    for (const Event& e : w->events) n += e.type == EventType::Attack;
    return n;
  }
};

// Cells of a list of "x,y" words, sorted like the engine's lists (ascending cell index).
inline std::string cellsText(const Grid& g, const std::vector<Cell>& cells) {
  std::vector<Cell> sorted = cells;
  std::sort(sorted.begin(), sorted.end());
  std::string out;
  for (Cell c : sorted) out += (out.empty() ? "" : " ") + std::to_string(g.x(c)) + "," + std::to_string(g.y(c));
  return out;
}

}  // namespace areatest
