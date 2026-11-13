#include <cstdio>

#include "core/data/loader.h"
#include "test.h"

using namespace ote;

static std::string game(const char* name) { return ote_test::sourceDir() + "/games/" + name; }
static std::string fixture(const char* name) { return ote_test::sourceDir() + "/tests/fixtures/" + name; }

static bool has(const std::string& hay, const char* needle) {
  return hay.find(needle) != std::string::npos;
}

// Expects loadGame to fail with a message containing every needle.
static void expectGameError(const char* fixtureName, const char* a, const char* b = "") {
  Database db;
  std::string err;
  bool ok = loadGame(fixture(fixtureName), db, err);
  CHECK(!ok);
  if (ok || !has(err, a) || !has(err, b)) std::printf("  [%s] message was: %s\n", fixtureName, err.c_str());
  CHECK(has(err, a));
  CHECK(has(err, b));
}

static void expectMapError(const char* mapId, const char* a, const char* b = "") {
  Database db;
  std::string err;
  CHECK(loadGame(fixture("base"), db, err));
  MapDef m;
  bool ok = loadMap(db, mapId, m, err);
  CHECK(!ok);
  if (ok || !has(err, a) || !has(err, b)) std::printf("  [%s] message was: %s\n", mapId, err.c_str());
  CHECK(has(err, a));
  CHECK(has(err, b));
}

TEST(loader_parse_color) {
  CHECK_EQ(int(parseColor("#3d85c6", 1)), 0x3d85c6);
  CHECK_EQ(int(parseColor("#FFFFFF", 1)), 0xffffff);
  CHECK_EQ(int(parseColor("3d85c6", 1)), 1);
  CHECK_EQ(int(parseColor("#3d85c", 1)), 1);
  CHECK_EQ(int(parseColor("#zzzzzz", 7)), 7);
  CHECK_EQ(int(parseColor("", 9)), 9);
}

TEST(loader_demo_srpg_game) {
  Database db;
  std::string err;
  CHECK(loadGame(game("demo-srpg"), db, err));
  CHECK_STR(err, "");
  CHECK_STR(db.name, "Emblem Demo");
  CHECK(db.rootPath.size() > 0 && db.rootPath.back() != '/');
  CHECK_EQ(db.stats.size(), size_t(6));
  CHECK_EQ(db.rules.statHp, db.findStat("hp"));
  CHECK_EQ(db.rules.statMove, db.findStat("mov"));
  CHECK_EQ(db.moveTypes.size(), size_t(3));
  CHECK_EQ(db.unitTypes.size(), size_t(7));
  CHECK_EQ(db.weapons.size(), size_t(4));
  CHECK_EQ(db.terrains.size(), size_t(6));
  CHECK_EQ(db.scripts.size(), size_t(1));

  const TerrainDef& forest = db.terrains[size_t(db.findTerrain("forest"))];
  CHECK_EQ(forest.moveCost.size(), size_t(3));
  CHECK_EQ(forest.moveCost[size_t(db.findMoveType("foot"))], 2);
  CHECK_EQ(forest.moveCost[size_t(db.findMoveType("horse"))], 3);
  CHECK_EQ(forest.moveCost[size_t(db.findMoveType("fly"))], 1);
  double v = 0;
  CHECK(propFind(forest.props, "avoid", v) && v == 20);
  const TerrainDef& mtn = db.terrains[size_t(db.findTerrain("mountain"))];
  CHECK(mtn.moveCost[size_t(db.findMoveType("horse"))] <= 0);
  const TerrainDef& water = db.terrains[size_t(db.findTerrain("water"))];
  CHECK(water.moveCost[size_t(db.findMoveType("foot"))] <= 0);
  CHECK_EQ(water.moveCost[size_t(db.findMoveType("fly"))], 1);

  const WeaponDef& bow = db.weapons[size_t(db.findWeapon("iron_bow"))];
  CHECK_EQ(bow.minRange, 2);
  CHECK_EQ(bow.maxRange, 2);
  CHECK(bow.hasVersusDefault);
  CHECK(bow.versusDefault == 1);
  CHECK_EQ(bow.versus.size(), size_t(1));
  CHECK_STR(bow.versus[size_t(0)].key, "flier");
  const WeaponDef& sword = db.weapons[size_t(db.findWeapon("iron_sword"))];
  CHECK(sword.counter && sword.afterMove && !sword.hasVersusDefault);
  CHECK_EQ(sword.maxRange, 1);

  const UnitTypeDef& peg = db.unitTypes[size_t(db.findUnitType("pegasus"))];
  CHECK_EQ(peg.moveType, db.findMoveType("fly"));
  CHECK_EQ(peg.stats.size(), db.stats.size());
  CHECK_EQ(peg.stats[size_t(db.findStat("mov"))], 7);
  CHECK_EQ(peg.growths[size_t(db.findStat("spd"))], 55);
  CHECK_EQ(peg.growths[size_t(db.findStat("mov"))], 0);  // missing = 0
  CHECK_EQ(peg.tags.size(), size_t(1));
  CHECK_EQ(peg.weapons.size(), size_t(1));
  CHECK_EQ(peg.weapons[size_t(0)], db.findWeapon("iron_lance"));
  CHECK(db.unitTypes[size_t(db.findUnitType("knight"))].tags[size_t(0)] == "armored");

  const RulesConfig& r = db.rules;
  CHECK(r.counterAttacks);
  CHECK(r.defeatNoUnits && r.defeatLeaderDead && !r.defeatLostHq);
  CHECK(r.progression && !r.economy && !r.production && !r.capture);
  CHECK_EQ(r.xpPerLevel, 100);
  CHECK_EQ(r.maxLevel, 20);
  CHECK_STR(r.fXpKill, "40 + math.max(0, b.level - a.level) * 10");
  CHECK_STR(r.fDamage, "srpg_damage(a, b, w, tb, vs)");
  CHECK_STR(r.fStrikes, "(a.spd - b.spd >= 4) and 2 or 1");
  CHECK(r.critMultiplier == 3);
  CHECK_EQ(db.maps.size(), size_t(1));
  CHECK_STR(db.maps[size_t(0)].file, "maps/bridge.json");
}

TEST(loader_demo_srpg_map) {
  Database db;
  std::string err;
  CHECK(loadGame(game("demo-srpg"), db, err));
  MapDef m;
  CHECK(loadMap(db, "bridge", m, err));
  CHECK_STR(m.id, "bridge");
  CHECK_STR(m.name, "Skirmish at the Ford");
  CHECK_EQ(m.width, 13);
  CHECK_EQ(m.height, 9);
  CHECK_EQ(m.terrain.size(), size_t(13 * 9));
  CHECK_EQ(m.heights.size(), size_t(13 * 9));
  CHECK_EQ(m.owner.size(), size_t(13 * 9));
  CHECK_EQ(m.owner[0], -1);
  CHECK_EQ(m.terrain[size_t(4 * 13 + 6)], db.findTerrain("bridge"));
  CHECK_EQ(m.sides.size(), size_t(2));
  CHECK_STR(m.sides[0].team, "blue");  // team defaults to id
  CHECK(!m.sides[0].ai && m.sides[1].ai);
  CHECK_EQ(int(m.sides[0].color), 0x3d85c6);
  CHECK_EQ(m.units.size(), size_t(9));

  int blue = 0, red = 0, leaders = 0;
  for (const MapUnitDef& u : m.units) {
    (u.side == 0 ? blue : red)++;
    CHECK(!u.name.empty());
    CHECK(u.level >= 1);
    CHECK_EQ(u.stats.size(), db.stats.size());
    if (u.leader) ++leaders;
  }
  CHECK_EQ(blue, 4);
  CHECK_EQ(red, 5);
  CHECK_EQ(leaders, 2);

  const MapUnitDef* gorm = nullptr;
  for (const MapUnitDef& u : m.units)
    if (u.name == "Gorm") gorm = &u;
  CHECK(gorm != nullptr);
  if (gorm) {
    CHECK_EQ(gorm->type, db.findUnitType("knight"));
    CHECK_EQ(gorm->stats[size_t(db.findStat("hp"))], 26);   // override
    CHECK_EQ(gorm->stats[size_t(db.findStat("str"))], 8);   // override
    CHECK_EQ(gorm->stats[size_t(db.findStat("def"))], 6);   // type base
    CHECK_EQ(m.terrain[size_t(gorm->y * m.width + gorm->x)], db.findTerrain("fort"));
    CHECK(gorm->leader);
  }

  CHECK(!loadMap(db, "nope", m, err));
  CHECK(has(err, "nope"));
}

TEST(loader_demo_economy_game) {
  Database db;
  std::string err;
  CHECK(loadGame(game("demo-economy"), db, err));
  CHECK_STR(db.name, "Wars Demo");
  CHECK_EQ(db.stats.size(), size_t(2));
  CHECK_EQ(db.resources.size(), size_t(1));
  CHECK_EQ(db.unitTypes.size(), size_t(5));
  CHECK_EQ(db.moveTypes.size(), size_t(3));

  const TerrainDef& forest = db.terrains[size_t(db.findTerrain("forest"))];
  CHECK_EQ(forest.moveCost[size_t(db.findMoveType("foot"))], 1);
  CHECK_EQ(forest.moveCost[size_t(db.findMoveType("tread"))], 2);
  CHECK_EQ(forest.moveCost[size_t(db.findMoveType("wheels"))], 3);
  const TerrainDef& mtn = db.terrains[size_t(db.findTerrain("mountain"))];
  CHECK_EQ(mtn.moveCost[size_t(db.findMoveType("foot"))], 2);
  CHECK(mtn.moveCost[size_t(db.findMoveType("tread"))] <= 0);

  const TerrainDef& factory = db.terrains[size_t(db.findTerrain("factory"))];
  CHECK_EQ(factory.capturePoints, 20);
  CHECK_EQ(factory.income.size(), size_t(1));
  CHECK_EQ(factory.income[size_t(0)].resource, db.findResource("funds"));
  CHECK_EQ(factory.income[size_t(0)].amount, 1000);
  CHECK_EQ(factory.produces.size(), size_t(5));
  const TerrainDef& hq = db.terrains[size_t(db.findTerrain("hq"))];
  CHECK_EQ(hq.tags.size(), size_t(1));
  CHECK_STR(hq.tags[size_t(0)], "hq");
  CHECK_EQ(db.terrains[size_t(db.findTerrain("sea"))].capturePoints, 0);

  const WeaponDef& cannon = db.weapons[size_t(db.findWeapon("cannon"))];
  CHECK_EQ(cannon.minRange, 2);
  CHECK_EQ(cannon.maxRange, 3);
  CHECK(!cannon.counter && !cannon.afterMove && !cannon.hasVersusDefault);
  CHECK_EQ(cannon.versus.size(), size_t(5));
  for (const WeaponDef& w : db.weapons) CHECK_EQ(w.versus.size(), size_t(5));

  const UnitTypeDef& tank = db.unitTypes[size_t(db.findUnitType("tank"))];
  CHECK_EQ(tank.cost[size_t(0)].amount, 7000);
  CHECK_EQ(tank.moveType, db.findMoveType("tread"));
  CHECK(!tank.canCapture);
  CHECK(db.unitTypes[size_t(db.findUnitType("infantry"))].canCapture);
  CHECK(db.unitTypes[size_t(db.findUnitType("mech"))].canCapture);
  for (const UnitTypeDef& u : db.unitTypes) CHECK_EQ(u.stats[size_t(db.rules.statHp)], 100);

  const RulesConfig& r = db.rules;
  CHECK(r.economy && r.production && r.capture && !r.progression);
  CHECK(!r.builtUnitsCanAct);
  CHECK(r.defeatNoUnits && r.defeatLostHq && !r.defeatLeaderDead);
  CHECK_STR(r.fCaptureAmount, "a.hp / 10");
  CHECK(r.fHit.empty() && r.fCrit.empty() && r.fStrikes.empty());
  CHECK(has(r.fDamage, "tb.defense"));
}

TEST(loader_demo_economy_map) {
  Database db;
  std::string err;
  CHECK(loadGame(game("demo-economy"), db, err));
  MapDef m;
  CHECK(loadMap(db, "twin_rivers", m, err));
  CHECK_STR(m.name, "Twin Rivers");
  CHECK_EQ(m.width, 15);
  CHECK_EQ(m.height, 10);
  CHECK_EQ(m.sides.size(), size_t(2));
  CHECK_EQ(m.sides[0].resources.size(), size_t(1));
  CHECK_EQ(m.sides[0].resources[size_t(0)].amount, 3000);
  CHECK_EQ(m.units.size(), size_t(6));

  // Mirror symmetry, and each side owns exactly one hq, factory and city.
  int hq[2] = {0, 0}, fac[2] = {0, 0}, city[2] = {0, 0}, neutral = 0;
  for (int y = 0; y < m.height; ++y) {
    for (int x = 0; x < m.width; ++x) {
      int c = y * m.width + x, mirror = y * m.width + (m.width - 1 - x);
      CHECK_EQ(m.terrain[size_t(c)], m.terrain[size_t(mirror)]);
      CHECK_EQ(m.owner[size_t(c)] < 0, m.owner[size_t(mirror)] < 0);
      const std::string& id = db.terrains[size_t(m.terrain[size_t(c)])].id;
      int o = m.owner[size_t(c)];
      if (o < 0) { if (id == "city") ++neutral; continue; }
      if (id == "hq") ++hq[o];
      if (id == "factory") ++fac[o];
      if (id == "city") ++city[o];
    }
  }
  for (int s = 0; s < 2; ++s) { CHECK_EQ(hq[s], 1); CHECK_EQ(fac[s], 1); CHECK_EQ(city[s], 1); }
  CHECK(neutral >= 4 && neutral <= 6);
  CHECK_EQ(m.owner[size_t(4 * m.width + 1)], 0);
  CHECK_EQ(m.owner[size_t(4 * m.width + 13)], 1);
}

TEST(loader_fixture_base_is_valid) {
  Database db;
  std::string err;
  CHECK(loadGame(fixture("base") + "/", db, err));  // trailing slash is stripped
  CHECK_STR(db.rootPath, fixture("base"));
  MapDef m;
  CHECK(loadMap(db, "ok", m, err));
  CHECK_STR(m.name, "ok");  // falls back to the MapRef name (= id)
  CHECK_EQ(m.sides[1].team == "b" ? 1 : 0, 1);
  CHECK_EQ(m.units[size_t(0)].level, 1);
  CHECK(m.units[size_t(0)].name.empty());
}

TEST(loader_errors_game) {
  expectGameError("dup_unit", "units.json", "grunt");
  expectGameError("no_roles", "game.json", "roles");
  expectGameError("bad_version", "game.json", "formatVersion");
  expectGameError("unknown_weapon", "units.json", "laser");
  expectGameError("bad_versus", "weapons.json", "dragon");
  expectGameError("bad_defeat", "game.json", "bogus");
  expectGameError("bad_turn_order", "game.json", "not supported");
  expectGameError("bad_range", "weapons.json", "bow");
}

TEST(loader_errors_missing_game) {
  Database db;
  std::string err;
  CHECK(!loadGame(fixture("no_such_game"), db, err));
  CHECK(has(err, "game.json"));
}

TEST(loader_errors_map) {
  expectMapError("bad_row", "bad_row.json", "row 1");
  expectMapError("bad_char", "'x'", "row 1, column 1");
  expectMapError("bad_legend", "lava", "legend");
  expectMapError("on_water", "water", "impassable");
  expectMapError("overlap", "shares a cell");
  expectMapError("bad_side", "green");
  expectMapError("bad_type", "hex");
  expectMapError("one_side", "two sides");
  expectMapError("out_of_bounds", "outside the map");
  expectMapError("missing_file", "nope.json");
}
