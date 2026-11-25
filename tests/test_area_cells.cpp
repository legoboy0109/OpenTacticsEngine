// Docs/SRPG.md "Area attacks and line fire": the one function that says which squares an area covers (grid/area.h) on square and hexagon
// grids, and the weapons.json keys "fire" / "area" with their refusals.
#include <algorithm>

#include "area_fixture.h"
#include "core/grid/area.h"

using namespace ote;
using areatest::cellsText;
using skilltest::Copy;
using skilltest::saysIn;

namespace {

AreaDef shape(AreaShape s, int radius = 0, int length = 0) {
  AreaDef a;
  a.shape = s;
  a.radius = radius;
  a.length = length;
  return a;
}

std::string covered(const Grid& g, int fx, int fy, int tx, int ty, const AreaDef& a) {
  std::vector<Cell> out;
  areaCells(g, g.at(fx, fy), g.at(tx, ty), a, out);
  CHECK(std::is_sorted(out.begin(), out.end()));
  CHECK(std::adjacent_find(out.begin(), out.end()) == out.end());  // no square twice
  return cellsText(g, out);
}

}  // namespace

TEST(area_cells_square_every_shape) {
  const Grid g(GridType::Square, 9, 9);
  // Burst: the square and its four neighbours.
  CHECK_STR(covered(g, 0, 4, 4, 4, shape(AreaShape::Burst, 1)), std::string("4,3 3,4 4,4 5,4 4,5"));
  // Radius 2: the diamond of 13.
  CHECK_STR(covered(g, 0, 4, 4, 4, shape(AreaShape::Radius, 2)), std::string("4,2 3,3 4,3 5,3 2,4 3,4 4,4 5,4 6,4 3,5 4,5 5,5 4,6"));
  // Cross 2: the square and two squares in each of the four directions (9).
  CHECK_STR(covered(g, 0, 4, 4, 4, shape(AreaShape::Cross, 2)), std::string("4,2 4,3 2,4 3,4 4,4 5,4 6,4 4,5 4,6"));
  // Line 3 from (0,4) through (2,4): the target and the two squares behind it; the squares between are not covered.
  CHECK_STR(covered(g, 0, 4, 2, 4, shape(AreaShape::Line, 0, 3)), std::string("2,4 3,4 4,4"));
  CHECK_STR(covered(g, 4, 0, 4, 2, shape(AreaShape::Line, 0, 3)), std::string("4,2 4,3 4,4"));
  CHECK_STR(covered(g, 8, 4, 6, 4, shape(AreaShape::Line, 0, 2)), std::string("5,4 6,4"));  // going west
  CHECK_STR(covered(g, 4, 8, 4, 6, shape(AreaShape::Line, 0, 1)), std::string("4,6"));      // length 1: just the target
  // A line aimed off the straight lines covers nothing; none = the target alone.
  CHECK_STR(covered(g, 0, 4, 3, 3, shape(AreaShape::Line, 0, 3)), std::string());
  CHECK_STR(covered(g, 0, 4, 3, 3, shape(AreaShape::None)), std::string("3,3"));
}

TEST(area_cells_square_are_clipped_by_the_map_edge) {
  const Grid g(GridType::Square, 9, 9);
  CHECK_STR(covered(g, 4, 4, 0, 0, shape(AreaShape::Radius, 2)), std::string("0,0 1,0 2,0 0,1 1,1 0,2"));
  CHECK_STR(covered(g, 4, 4, 0, 0, shape(AreaShape::Cross, 2)), std::string("0,0 1,0 2,0 0,1 0,2"));
  CHECK_STR(covered(g, 0, 4, 7, 4, shape(AreaShape::Line, 0, 3)), std::string("7,4 8,4"));
  CHECK_STR(covered(g, 4, 4, 8, 8, shape(AreaShape::Burst, 1)), std::string("8,7 7,8 8,8"));
}

TEST(area_cells_straight_direction_square) {
  const Grid g(GridType::Square, 9, 9);
  int steps = 0;
  CHECK_EQ(straightDirection(g, g.at(2, 2), g.at(2, 0), &steps), 0);  // north
  CHECK_EQ(steps, 2);
  CHECK_EQ(straightDirection(g, g.at(2, 2), g.at(5, 2), &steps), 1);  // east
  CHECK_EQ(steps, 3);
  CHECK_EQ(straightDirection(g, g.at(2, 2), g.at(3, 3)), -1);  // a diagonal is no straight line on a square grid
  CHECK_EQ(straightDirection(g, g.at(2, 2), g.at(2, 2)), -1);
  CHECK_EQ(straightDirection(g, g.at(2, 2), kNoCell), -1);
}

// Hexagons, both layouts: every shape is built here from neighbour() steps, so the test does not repeat the grid's own coordinate maths.
TEST(area_cells_hex_every_shape_both_layouts) {
  for (HexLayout layout : {HexLayout::Pointy, HexLayout::Flat}) {
    const Grid g(GridType::Hex, 11, 11, layout);
    const Cell mid = g.at(5, 5);
    std::vector<Cell> out;
    // Burst: the hexagon and its six neighbours.
    std::vector<Cell> want{mid};
    for (int d = 0; d < 6; ++d) want.push_back(g.neighbor(mid, d));
    areaCells(g, g.at(0, 5), mid, shape(AreaShape::Burst, 1), out);
    CHECK_STR(cellsText(g, out), cellsText(g, want));
    CHECK_EQ((int)out.size(), 7);
    // Radius 2: 19 hexagons, every one within 2 and none missing.
    areaCells(g, g.at(0, 5), mid, shape(AreaShape::Radius, 2), out);
    CHECK_EQ((int)out.size(), 19);
    for (Cell c : out) CHECK(g.distance(mid, c) <= 2);
    // Radius 3: 37.
    areaCells(g, g.at(0, 5), mid, shape(AreaShape::Radius, 3), out);
    CHECK_EQ((int)out.size(), 37);
    // Cross 2: the hexagon plus two in each of the six directions (13).
    want.assign(1, mid);
    for (int d = 0; d < 6; ++d) {
      Cell c = mid;
      for (int k = 0; k < 2; ++k) want.push_back(c = g.neighbor(c, d));
    }
    areaCells(g, g.at(0, 5), mid, shape(AreaShape::Cross, 2), out);
    CHECK_STR(cellsText(g, out), cellsText(g, want));
    CHECK_EQ((int)out.size(), 13);
    // Line 3, in each of the six directions: shot from a square two steps off, aimed at `mid`: mid and the two behind it.
    for (int d = 0; d < 6; ++d) {
      const Cell from = g.neighbor(g.neighbor(mid, g.opposite(d)), g.opposite(d));
      int steps = 0;
      CHECK_EQ(straightDirection(g, from, mid, &steps), d);
      CHECK_EQ(steps, 2);
      want.clear();
      Cell c = mid;
      want.push_back(c);
      want.push_back(c = g.neighbor(c, d));
      want.push_back(c = g.neighbor(c, d));
      areaCells(g, from, mid, shape(AreaShape::Line, 0, 3), out);
      CHECK_STR(cellsText(g, out), cellsText(g, want));
    }
    // A square a bent two steps away (one step in one direction, then another direction) is not on a straight line.
    const Cell bent = g.neighbor(g.neighbor(mid, 0), 1);
    CHECK_EQ(straightDirection(g, mid, bent), -1);
    areaCells(g, mid, bent, shape(AreaShape::Line, 0, 3), out);
    CHECK(out.empty());
  }
}

// ---- weapons.json ----

TEST(area_weapons_load_with_their_numbers) {
  Copy c("area_load", "area-fire");
  CHECK_STR(c.gameError(), "");
  Database db;
  std::string err;
  CHECK(loadGame(c.dir, db, err));
  const WeaponDef& rifle = db.weapons[(size_t)db.findWeapon("rifle")];
  CHECK(rifle.fire == FireMode::Line && !rifle.area.on() && rifle.aimsStraight());
  const WeaponDef& sword = db.weapons[(size_t)db.findWeapon("sword")];
  CHECK(sword.fire == FireMode::Direct && !sword.area.on() && !sword.aimsStraight());
  const WeaponDef& bomb = db.weapons[(size_t)db.findWeapon("bomb")];
  CHECK(bomb.area.shape == AreaShape::Burst && bomb.area.radius == 1 && !bomb.area.friendlyFire);
  const WeaponDef& mortar = db.weapons[(size_t)db.findWeapon("mortar")];
  CHECK(mortar.area.shape == AreaShape::Radius && mortar.area.radius == 2);
  const WeaponDef& beam = db.weapons[(size_t)db.findWeapon("beam")];
  CHECK(beam.area.shape == AreaShape::Line && beam.area.length == 3 && beam.aimsStraight());
  CHECK(db.weapons[(size_t)db.findWeapon("cross")].area.shape == AreaShape::Cross);
  CHECK(db.weapons[(size_t)db.findWeapon("flak")].area.friendlyFire);
  const WeaponDef& rail = db.weapons[(size_t)db.findWeapon("rail")];
  CHECK(rail.fire == FireMode::Line && rail.area.shape == AreaShape::Line);
  CHECK(db.straightWeapons);
}

TEST(area_weapons_loader_refuses_bad_data_naming_file_entry_and_key) {
  auto bad = [](const char* name, const char* from, const char* to, const char* wanted) {
    Copy c(name, "area-fire");
    c.swap("data/weapons.json", from, to);
    const std::string err = c.gameError();
    CHECK(saysIn(err, "weapons.json"));
    CHECK(saysIn(err, wanted));
  };
  bad("area_b1", "\"shape\": \"burst\"", "\"shape\": \"cone\"", "\"area\".\"shape\" must be \"burst\", \"line\", \"radius\", \"cross\", \"ring\" or \"field\" (not \"cone\")");
  bad("area_b2", "\"shape\": \"radius\", \"radius\": 2", "\"shape\": \"radius\"", "needs a \"radius\"");
  bad("area_b3", "\"radius\": 2 }, \"props\": { \"might\": 4 } },\n  { \"id\": \"beam\"", "\"radius\": 9 }, \"props\": { \"might\": 4 } },\n  { \"id\": \"beam\"",
      "\"radius\" must be from 1 to 8");
  bad("area_b4", "\"shape\": \"line\", \"length\": 3", "\"shape\": \"line\"", "needs a \"length\"");
  bad("area_b5", "\"shape\": \"line\", \"length\": 3", "\"shape\": \"line\", \"length\": 0", "\"length\" must be from 1 to 8");
  bad("area_b6", "\"shape\": \"burst\"", "\"shape\": \"burst\", \"length\": 2", "\"length\" belongs to the shape \"line\" only");
  bad("area_b7", "\"shape\": \"line\", \"length\": 3", "\"shape\": \"line\", \"length\": 3, \"radius\": 2", "\"radius\" belongs to the shapes");
  bad("area_b8", "\"shape\": \"burst\"", "\"shape\": \"burst\", \"colour\": 1", "unknown key \"colour\"");
  bad("area_b9", "\"fire\": \"line\", \"props\": { \"might\": 5 }", "\"fire\": \"arc\", \"props\": { \"might\": 5 }", "\"fire\" must be \"direct\" or \"line\" (not \"arc\")");
  bad("area_b10", "\"radius\": 1, \"friendlyFire\": true", "\"radius\": 1, \"friendlyFire\": \"yes\"", "friendlyFire");
  bad("area_b11", "\"radius\": 2 }, \"props\": { \"might\": 4 } },\n  { \"id\": \"beam\"", "\"radius\": 2.5 }, \"props\": { \"might\": 4 } },\n  { \"id\": \"beam\"",
      "\"radius\"");
  bad("area_b12", "\"area\": { \"shape\": \"burst\" }", "\"area\": \"burst\"", "\"area\"");
  bad("area_b13", "\"area\": { \"shape\": \"burst\" }", "\"area\": {}", "\"area\" needs a \"shape\"");
  // The entry is named: the weapon's id is in the message.
  Copy c("area_named", "area-fire");
  c.swap("data/weapons.json", "\"shape\": \"burst\"", "\"shape\": \"cone\"");
  CHECK(saysIn(c.gameError(), "weapon \"bomb\""));
}

TEST(area_keys_off_leave_the_weapon_a_plain_one) {
  Copy c("area_plain", "area-fire");
  c.put("data/weapons.json", "{ \"weapons\": [ { \"id\": \"sword\", \"props\": { \"might\": 4 } } ] }");
  c.put("data/units.json", "{ \"units\": [ { \"id\": \"lancer\", \"moveType\": \"foot\", \"stats\": { \"hp\": 30, \"str\": 6, \"def\": 2, \"mov\": 3 }, \"weapons\": [\"sword\"] } ] }");
  Database db;
  std::string err;
  CHECK(loadGame(c.dir, db, err));
  CHECK(!db.straightWeapons);
  CHECK(db.weapons[0].fire == FireMode::Direct && !db.weapons[0].area.on());
}
