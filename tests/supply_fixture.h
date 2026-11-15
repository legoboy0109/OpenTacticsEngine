// Helpers for the repair and supply tests (docs/ECONOMY.md): the miniature game of battle_fixture.h plus
//   weapon  gun (index 3): range 1..2, ammunition 3, hits anything     cannon (tank's): ammunition 2
//   units   plane (3)  hp10 atk5 def2 mov5, gun, fuel 20 burning 4 a turn, cost 200, tag "air"
//           truck (4)  hp10 atk0 def0 mov4, no weapon, fuel 30, supplies, cost 150
//           ranger (5) hp10 atk5 def2 mov3, gun then sword, cost 100
//   rules   repair 20% at costShare 100, own structures resupply
//   structures (index after the fixture's 7): hospital (8th, repairs everybody), workshop (repairs "armor" = tanks),
//           airfield (repairs "air")
// Everything is built in code. The world is NOT started: tweak db / map, then begin() or play().
// Units start with full fuel and ammunition; note that the first turn has already begun when begin() returns,
// so planes have burnt their first 4 fuel (16 left).
#pragma once
#include "story_fixture.h"

namespace suptest {

using namespace ote;

enum { Gun = 3 };                             // db.weapons
enum { Plane = 3, Truck = 4, Ranger = 5 };    // db.unitTypes
enum { Hospital = 7, Workshop, Airfield };    // db.structures

inline void addSupply(Database& db) {
  WeaponDef gun;
  gun.id = gun.name = "gun";
  gun.minRange = 1;
  gun.maxRange = 2;
  gun.ammo = 3;
  db.weapons.push_back(gun);
  db.weapons[2].ammo = 2;  // the tank's cannon
  UnitTypeDef plane; plane.id = plane.name = "plane"; plane.glyph = 'P'; plane.stats = {10, 5, 2, 5};
  plane.growths = {0, 0, 0, 0}; plane.weapons = {Gun}; plane.cost = {{0, 200}}; plane.tags = {"air"};
  plane.fuel = 20; plane.fuelPerTurn = 4;
  UnitTypeDef truck; truck.id = truck.name = "truck"; truck.glyph = 'U'; truck.stats = {10, 0, 0, 4};
  truck.growths = {0, 0, 0, 0}; truck.cost = {{0, 150}}; truck.fuel = 30; truck.supplies = true;
  UnitTypeDef ranger; ranger.id = ranger.name = "ranger"; ranger.glyph = 'R'; ranger.stats = {10, 5, 2, 3};
  ranger.growths = {0, 0, 0, 0}; ranger.weapons = {Gun, 0}; ranger.cost = {{0, 100}};
  ranger.tags = {"soldier"};  // the sword's versus table knows this tag
  db.unitTypes.push_back(plane);
  db.unitTypes.push_back(truck);
  db.unitTypes.push_back(ranger);
  auto kind = [](const char* id) {
    StructureDef s;
    s.id = s.name = id;
    s.moveCost = {-1};
    return s;
  };
  StructureDef hospital = kind("hospital");
  StructureDef workshop = kind("workshop"); workshop.repairsAll = false; workshop.repairs = {"armor"};
  StructureDef airfield = kind("airfield"); airfield.repairsAll = false; airfield.repairs = {"air"};
  db.structures.push_back(hospital);
  db.structures.push_back(workshop);
  db.structures.push_back(airfield);
  db.rules.supply.repair = true;
  db.rules.supply.repairAmount = 20;
  db.rules.supply.repairCostShare = 100;
  db.rules.supply.structures = true;
}

struct SupplyWorld : story::StoryWorld {
  // Same arguments as the fixture's World, but unit types may be any of the added ones.
  SupplyWorld(const std::vector<std::string>& rows, const std::vector<fixture::UnitSpec>& units,
              const std::vector<const char*>& ids = {})
      : StoryWorld(rows, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 1, 0, false}}, {}) {
    addSupply(db);
    map = fixture::makeMap(db, rows, units);
    for (size_t i = 0; i < ids.size(); ++i) map.units[i].id = ids[i];
    map.sides[0].resources = {{0, 1000}};
    map.sides[1].resources = {{0, 1000}};
  }
  int funds(int side = 0) const { return battle.sides[(size_t)side].resources[0]; }
  int unitIdAt(int x, int y) const { return battle.unitAt(at(x, y)); }
  Unit& unit(int x, int y) { return battle.units[(size_t)unitIdAt(x, y)]; }
  // Both sides end their turns: blue's next turn has begun.
  bool nextTurn() { return act(Action::endTurn()) && act(Action::endTurn()); }
  std::string refused(const Action& a) {
    std::string why;
    if (battle.validate(a, &why)) return "";
    return why.empty() ? "refused" : why;
  }
  // Events of one type, in order.
  std::vector<Event> all(EventType t) const {
    std::vector<Event> out;
    for (const Event& e : events)
      if (e.type == t) out.push_back(e);
    return out;
  }
};

// A 7 x 3 field of plain ground.
inline std::vector<std::string> field() { return {".......", ".......", "......."}; }

}  // namespace suptest
