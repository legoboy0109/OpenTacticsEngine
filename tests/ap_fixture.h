// Shared by the action-point tests (docs/SRPG.md "Action points"): a scene of tests/fixtures/action-points with a map written for the test.
//
// Numbers of the fixture: blue starts every turn with min(left, 10) + 20 AP, red the same on its own turns (the first blue turn: 20). A move
// costs 1 AP per movement point (woods 2 points), the first 4 points a unit walks in a turn at that price, the rest double; one attack per
// unit and turn costing 2 AP, the poker's spear 1, the mortar 3 + 1 for its area. Move: lancer 4, archer 3, poker 4, mortarman 3, scout 6
// (but its own price: 2 AP a point, cap 2), dummy 3, tank 3. Damage = str + might - def (lancer sword 8 against a dummy), dummy hp 40.
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "area_fixture.h"

namespace aptest {

using namespace ote;
using areatest::plain;
using areatest::U;
using skilltest::Copy;
using skilltest::World;

struct Scene {
  Copy c;
  std::unique_ptr<World> w;
  Scene(const std::string& name, const std::vector<std::string>& rows, const std::vector<U>& units, bool bothAi = false,
        const std::function<void(Copy&)>& tweak = nullptr)
      : c(name, "action-points") {
    c.put("maps/field.json", areatest::mapJson((int)rows[0].size(), (int)rows.size(), rows, units, bothAi));
    if (tweak) tweak(c);
    w.reset(new World("field", c.dir));
  }
  // A whole map written by the test (structures, resources...).
  Scene(const std::string& name, const std::string& mapText, const std::function<void(Copy&)>& tweak = nullptr) : c(name, "action-points") {
    c.put("maps/field.json", mapText);
    if (tweak) tweak(c);
    w.reset(new World("field", c.dir));
  }
  Battle& b() { return w->b(); }
  const Database& db() { return w->db(); }
  Cell at(int x, int y) { return b().grid.at(x, y); }
  int weapon(const char* id) { return db().findWeapon(id); }
  int id(const char* story) { return w->id(story); }
  Unit& unit(const char* story) { return w->unit(story); }
  int apIndex() { return db().findResource("ap"); }
  // The pool of a side (0 = blue, 1 = red).
  int ap(int side = 0) { return b().sides[(size_t)side].resources[(size_t)apIndex()]; }
  int gold(int side = 0) { return b().sides[(size_t)side].resources[(size_t)db().findResource("gold")]; }
  bool go(const char* who, int x, int y, std::string* why = nullptr) { return w->apply(Action::move(id(who), at(x, y)), why); }
  bool hit(const char* who, const char* target, const char* weaponId, std::string* why = nullptr) {
    return w->apply(Action::attack(id(who), id(target), weapon(weaponId)), why);
  }
  // The refusal text of an action that must not go through.
  std::string refused(const Action& a) {
    std::string why;
    return b().validate(a, &why) ? std::string("(allowed)") : why;
  }
  std::string refusedMove(const char* who, int x, int y) { return refused(Action::move(id(who), at(x, y))); }
  bool endTurn() { return w->apply(Action::endTurn()); }
};

// The fixture game with its "actionPoints" block cut out (refill and the rest stay): a game that only uses the refill.
// `andTheirKeys`: also the keys of weapons and unit types that only an action-point game accepts (so the game still loads).
inline void dropActionPoints(Copy& c, bool andTheirKeys = false) {
  if (andTheirKeys) {
    c.swap("data/weapons.json", ", \"apCost\": 1", "");
    c.swap("data/weapons.json", ", \"apCost\": 3", "");
    c.swap("data/weapons.json", ", \"apExtra\": 1", "");
    c.swap("data/units.json", ", \"ap\": { \"step\": 2, \"softCap\": 2 }", "");
  }
  std::string g = c.read("game.json");
  const size_t from = g.find(",\n             \"actionPoints\"");
  const std::string tail = "\"attacksPerTurn\": 1 } }";
  const size_t to = g.find(tail, from) + tail.size();
  c.put("game.json", g.substr(0, from) + g.substr(to));
}

// The map of the build and research tests: blue's factory at 0,4 and a lancer, 20 gold each.
inline std::string factoryMap() {
  return "{ \"name\": \"T\", \"grid\": { \"type\": \"square\", \"width\": 12, \"height\": 5 }, \"legend\": { \".\": \"plain\" },\n"
         "  \"tiles\": [\"............\", \"............\", \"............\", \"............\", \"............\"],\n"
         "  \"sides\": [ { \"id\": \"blue\", \"controller\": \"human\", \"resources\": { \"gold\": 20 } },"
         " { \"id\": \"red\", \"controller\": \"ai\", \"resources\": { \"gold\": 20 } } ],\n"
         "  \"structures\": [ { \"type\": \"factory\", \"x\": 0, \"y\": 4, \"side\": \"blue\" }, { \"type\": \"factory\", \"x\": 11, \"y\": 0, \"side\": \"red\" } ],\n"
         "  \"units\": [ { \"type\": \"lancer\", \"side\": \"blue\", \"x\": 1, \"y\": 2, \"id\": \"lan\", \"leader\": true },"
         " { \"type\": \"dummy\", \"side\": \"red\", \"x\": 9, \"y\": 2, \"id\": \"foe\", \"leader\": true } ] }";
}

}  // namespace aptest
