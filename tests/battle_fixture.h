// In-code miniature game used by the battle tests, so they do not depend
// on the file loader or on demo game balance.
//
// Stats: hp, atk, def, mov. Terrain legend:
//   '.' plain   '#' wall (impassable)   'f' forest (cost 2, defense 1)
//   'c' city (capturable 20, income 100)   'H' hq (capturable 20, tag hq)
//   'F' factory (produces soldier)
// Unit types: soldier (hp10 atk5 def2 mov3, sword r1, can capture, cost 100)
//             archer  (hp8 atk4 def1 mov3, bow r2 only, no counter)
//             tank    (hp20 atk8 def5 mov4, tagged "armor"; swords can't hurt it)
#pragma once
#include "core/battle/battle.h"
#include "core/script/formulas.h"
#include "core/base/log.h"
#include "test.h"

namespace fixture {

using namespace ote;

inline Database makeDb() {
  Database db;
  db.name = "fixture";
  db.stats = {{"hp", "HP"}, {"atk", "Atk"}, {"def", "Def"}, {"mov", "Move"}};
  db.resources = {{"funds", "Funds"}};
  db.moveTypes = {{"foot", "Foot"}};
  db.rules.statHp = 0;
  db.rules.statMove = 3;
  db.rules.fDamage = "a.atk - b.def - (tb.defense or 0)";

  WeaponDef sword; sword.id = "sword"; sword.name = "Sword";
  sword.versus = {{"soldier", 1}, {"archer", 1}};  // no default: cannot hit tanks
  WeaponDef bow; bow.id = "bow"; bow.name = "Bow"; bow.minRange = 2; bow.maxRange = 2; bow.counter = false;
  WeaponDef cannon; cannon.id = "cannon"; cannon.name = "Cannon";
  db.weapons = {sword, bow, cannon};

  UnitTypeDef soldier; soldier.id = "soldier"; soldier.name = "Soldier"; soldier.glyph = 'S';
  soldier.stats = {10, 5, 2, 3}; soldier.growths = {100, 100, 0, 0};
  soldier.weapons = {0}; soldier.cost = {{0, 100}}; soldier.canCapture = true;
  UnitTypeDef archer; archer.id = "archer"; archer.name = "Archer"; archer.glyph = 'A';
  archer.stats = {8, 4, 1, 3}; archer.growths = {0, 0, 0, 0}; archer.weapons = {1};
  UnitTypeDef tank; tank.id = "tank"; tank.name = "Tank"; tank.glyph = 'T';
  tank.stats = {20, 8, 5, 4}; tank.growths = {0, 0, 0, 0}; tank.weapons = {2}; tank.tags = {"armor"};
  db.unitTypes = {soldier, archer, tank};

  auto terrain = [](const char* id, int cost) {
    TerrainDef t; t.id = id; t.name = id; t.moveCost = {cost}; return t;
  };
  TerrainDef plain = terrain("plain", 1);
  TerrainDef wall = terrain("wall", 0);
  TerrainDef forest = terrain("forest", 2); forest.props = {{"defense", 1}};
  TerrainDef city = terrain("city", 1); city.capturePoints = 20; city.income = {{0, 100}};
  TerrainDef hq = terrain("hq", 1); hq.capturePoints = 20; hq.tags = {"hq"};
  TerrainDef factory = terrain("factory", 1); factory.produces = {0};
  db.terrains = {plain, wall, forest, city, hq, factory};
  return db;
}

struct UnitSpec { const char* type; int side; int x, y; bool leader; };

// rows: equal-length strings using the legend above. Two sides: 0 "blue"
// (human), 1 "red" (ai), enemies.
inline MapDef makeMap(const Database& db, const std::vector<std::string>& rows,
                      const std::vector<UnitSpec>& units) {
  MapDef m;
  m.id = m.name = "test";
  m.height = (int)rows.size();
  m.width = (int)rows[0].size();
  for (const std::string& row : rows)
    for (char ch : row) {
      const char* id = ch == '#' ? "wall" : ch == 'f' ? "forest" : ch == 'c' ? "city"
                     : ch == 'H' ? "hq" : ch == 'F' ? "factory" : "plain";
      m.terrain.push_back(db.findTerrain(id));
    }
  m.owner.assign(m.terrain.size(), -1);
  MapSideDef blue; blue.id = "blue"; blue.name = "Blue";
  MapSideDef red; red.id = "red"; red.name = "Red"; red.ai = true;
  m.sides = {blue, red};
  for (const UnitSpec& s : units) {
    MapUnitDef u;
    u.type = db.findUnitType(s.type);
    u.side = s.side;
    u.x = s.x; u.y = s.y;
    u.leader = s.leader;
    u.stats = db.unitTypes[u.type].stats;
    m.units.push_back(u);
  }
  return m;
}

// Owns everything a Battle points at. Tweak `db`/`map` then call begin().
struct World {
  Database db = makeDb();
  MapDef map;
  Formulas formulas;
  Battle battle;
  std::vector<Event> events;

  World(const std::vector<std::string>& rows, const std::vector<UnitSpec>& units) {
    map = makeMap(db, rows, units);
  }
  bool begin(uint32_t seed = 1) {
    std::string err;
    if (!formulas.init(db, err)) { std::printf("  formulas: %s\n", err.c_str()); return false; }
    if (!battle.init(db, map, formulas, seed, err)) { std::printf("  init: %s\n", err.c_str()); return false; }
    battle.start(events);
    return true;
  }
  bool act(const Action& a) { return battle.apply(a, events); }
  Cell at(int x, int y) const { return battle.grid.at(x, y); }
  // Number of events of a type recorded so far.
  int count(EventType type) const {
    int n = 0;
    for (const Event& e : events) if (e.type == type) ++n;
    return n;
  }
  const Event* last(EventType type) const {
    for (size_t i = events.size(); i-- > 0;) if (events[i].type == type) return &events[i];
    return nullptr;
  }
};

}  // namespace fixture
