// "Fidelity 2": the battle scene's backdrop and platforms per map (docs/BATTLE_SCENE.md), weather and movement cost (docs/ECONOMY.md), which
// side each fighter stands on. Game: tests/fidelity2_fixture.h.
#include "battle/fight_script.h"
#include "core/data/battle_pick.h"
#include "fidelity2_fixture.h"

using namespace ote;
using namespace f2;

namespace {

const char* kBattle =
    "{ \"backgrounds\": { \"a\": {\"sheet\":\"bgs\",\"cell\":[0,0]}, \"b\": {\"sheet\":\"bgs\",\"cell\":[1,0]}, \"c\": {\"sheet\":\"bgs\",\"cell\":[1,0]}, \"d\": {\"sheet\":\"bgs\",\"cell\":[0,0]} },"
    "  \"terrainBackground\": { \"plain\": \"a\" }, \"defaultBackground\": \"b\","
    "  \"platforms\": { \"p1\": {\"sheet\":\"plat\",\"cell\":[0,0],\"far\":[1,0]}, \"p2\": {\"sheet\":\"plat\",\"cell\":[1,0]} },"
    "  \"terrainPlatform\": { \"plain\": \"p1\" }, \"defaultPlatform\": \"p2\" }";

Opts art(const std::string& mapExtra = "", const std::string& gameExtra = "") {
  Opts o;
  o.graphics = kBattle;
  o.mapExtra = mapExtra;
  o.gameExtra = gameExtra;
  return o;
}

struct Loaded {
  Database db;
  MapDef map;
  std::string err;
  bool ok = false;
  explicit Loaded(const std::string& root) { ok = loadGame(root, db, err) && loadMap(db, "arena", map, err); }
  std::string bg(const char* terrain) const { int i = pickBattleBackground(db, map, db.findTerrain(terrain)); return i < 0 ? "" : db.graphics.battle.backgrounds[(size_t)i].id; }
  std::string plat(const char* terrain) const { int i = pickBattlePlatform(db, map, db.findTerrain(terrain)); return i < 0 ? "" : db.graphics.battle.platforms[(size_t)i].id; }
};

}  // namespace

// ---- item 3: backdrop and platforms ----

TEST(fid2_backdrop_without_map_keys_is_the_game_wide_table) {
  Loaded l(make("bg_game", art()));
  CHECK(l.ok);
  CHECK_STR(l.bg("plain"), "a");
  CHECK_STR(l.bg("swamp"), "b");   // the game's default
  CHECK_STR(l.plat("plain"), "p1");
  CHECK_STR(l.plat("swamp"), "p2");
  CHECK(!l.db.battleBackgroundFromAttacker);
  CHECK(l.db.graphics.battle.platforms[0].hasFar);
}

TEST(fid2_a_map_table_wins_over_the_game_wide_table) {
  Loaded l(make("bg_map", art("\"battleBackgrounds\": { \"swamp\": \"c\", \"default\": \"d\" }, \"battlePlatforms\": { \"swamp\": \"p1\" },")));
  CHECK(l.ok);
  CHECK_STR(l.bg("swamp"), "c");
  CHECK_STR(l.bg("plain"), "d");   // the map's default beats the game's entry for plain
  CHECK_STR(l.plat("swamp"), "p1");
  CHECK_STR(l.plat("plain"), "p1");  // (no map default: the game's table)
  // The map's single picture stays under its table's entries and above its default.
  Loaded m(make("bg_map2", art("\"battleBackground\": \"b\", \"battleBackgrounds\": { \"swamp\": \"c\", \"default\": \"d\" },")));
  CHECK(m.ok);
  CHECK_STR(m.bg("swamp"), "c");
  CHECK_STR(m.bg("plain"), "b");
}

TEST(fid2_backdrop_loader_refusals) {
  CHECK(says(Loaded(make("bg_r1", art("\"battleBackgrounds\": { \"lava\": \"a\" },"))).err, "no terrain with this id"));
  CHECK(says(Loaded(make("bg_r2", art("\"battleBackgrounds\": { \"plain\": \"zzz\" },"))).err, "there is no background \"zzz\""));
  CHECK(says(Loaded(make("bg_r3", art("\"battleBackgrounds\": {},"))).err, "\"battleBackgrounds\" must be"));
  CHECK(says(Loaded(make("bg_r4", art("\"battleBackgrounds\": \"a\","))).err, "\"battleBackgrounds\" must be"));
  CHECK(says(Loaded(make("bg_r5", art("\"battlePlatforms\": { \"plain\": \"zzz\" },"))).err, "there is no platform \"zzz\""));
  CHECK(says(Loaded(make("bg_r6", art("\"battlePlatforms\": { \"default\": 3 },"))).err, "must be the id of a platform"));
  CHECK(says(Loaded(make("bg_r7", art("", "\"battleBackgroundFrom\": \"both\","))).err, "\"battleBackgroundFrom\" must be"));
  Opts o = art();
  o.graphics = "{ \"platforms\": { \"p\": {\"sheet\":\"nope\",\"cell\":[0,0]} } }";
  CHECK(says(Loaded(make("bg_r8", o)).err, "unknown sheet \"nope\""));
  o.graphics = "{ \"platforms\": { \"p\": {\"sheet\":\"plat\",\"cell\":[0,0],\"far\":[9,0]} } }";
  CHECK(says(Loaded(make("bg_r9", o)).err, "outside sheet"));
  o.graphics = "{ \"platforms\": { \"default\": {\"sheet\":\"plat\",\"cell\":[0,0]} } }";
  CHECK(says(Loaded(make("bg_r10", o)).err, "cannot be \"default\""));
  o.graphics = "{ \"platforms\": { \"p\": {\"sheet\":\"plat\",\"cell\":[0,0]} }, \"terrainPlatform\": { \"plain\": \"q\" } }";
  CHECK(says(Loaded(make("bg_r11", o)).err, "there is no platform \"q\""));
  o.graphics = "{ \"platforms\": { \"p\": {\"sheet\":\"plat\",\"cell\":[0,0],\"glow\":1} } }";
  CHECK(says(Loaded(make("bg_r12", o)).err, "unknown key \"glow\""));
}

TEST(fid2_backdrop_from_key) {
  Loaded d(make("bg_from1", art("", "\"battleBackgroundFrom\": \"defender\",")));
  CHECK(d.ok && !d.db.battleBackgroundFromAttacker);
  Loaded a(make("bg_from2", art("", "\"battleBackgroundFrom\": \"attacker\",")));
  CHECK(a.ok && a.db.battleBackgroundFromAttacker);
}

// ---- item 7: which side each fighter stands on ----

TEST(fid2_scene_sides_rule) {
  // mode 0 = "map": the unit nearer the left edge, the initiator on a tie
  CHECK(initiatorStandsLeft(0, true, false, 2, 5));
  CHECK(!initiatorStandsLeft(0, true, false, 5, 2));
  CHECK(initiatorStandsLeft(0, false, true, 3, 3));
  // mode 1 = "playerRight": a person's fighter on the right
  CHECK(!initiatorStandsLeft(1, true, false, 2, 5));   // the player attacks: it stands right, whatever the map says
  CHECK(initiatorStandsLeft(1, false, true, 5, 2));    // the computer attacks the player: the computer stands left
  // mode 2 = "playerLeft": the opposite
  CHECK(initiatorStandsLeft(2, true, false, 5, 2));
  CHECK(!initiatorStandsLeft(2, false, true, 2, 5));
  // both or neither person: the map rule
  CHECK(initiatorStandsLeft(1, true, true, 2, 5));
  CHECK(!initiatorStandsLeft(1, true, true, 5, 2));
  CHECK(initiatorStandsLeft(2, false, false, 2, 5));
  CHECK(!initiatorStandsLeft(2, false, false, 5, 2));
}

TEST(fid2_scene_sides_script_follows_the_rule) {
  // One strike by A (unit 1) on B (unit 2); A at column 6, B at column 1: the map rule puts B on the left.
  Event strike;
  strike.type = EventType::Attack;
  strike.a = 1;
  strike.b = 2;
  strike.flags = kHit;
  strike.d = 3;
  FightInfo info;
  FightUnitInfo a, b;
  a.unit = 1; a.name = "A"; a.hp = 10; a.maxHp = 10; a.x = 6; a.human = true;
  b.unit = 2; b.name = "B"; b.hp = 10; b.maxHp = 10; b.x = 1; b.human = false;
  info.units[1] = a;
  info.units[2] = b;
  for (int mode = 0; mode < 3; ++mode) {
    info.sidesMode = mode;
    const FightScript s = buildFightScript({strike}, info);
    CHECK(s.valid);
    // map: B left; playerRight: the human A is on the right (B left); playerLeft: A left.
    CHECK_EQ(s.side[0].unit, mode == 2 ? 1 : 2);
    CHECK_EQ(s.side[1].unit, mode == 2 ? 2 : 1);
    CHECK_EQ(s.side[s.initiator].unit, 1);
  }
}

TEST(fid2_scene_sides_game_key) {
  Opts o;
  o.gameExtra = "\"battleSceneSides\": \"playerRight\",";
  Database db;
  std::string err;
  CHECK(loadGame(make("sides1", o), db, err));
  CHECK_EQ(db.battleSceneSides, 1);
  o.gameExtra = "\"battleSceneSides\": \"playerLeft\",";
  CHECK(loadGame(make("sides2", o), db, err));
  CHECK_EQ(db.battleSceneSides, 2);
  CHECK(loadGame(make("sides0"), db, err));
  CHECK_EQ(db.battleSceneSides, 0);
  o.gameExtra = "\"battleSceneSides\": \"center\",";
  CHECK(says(error(make("sides3", o)), "\"battleSceneSides\" must be \"map\""));
}

// ---- item 4: weather and movement cost ----

namespace {

Opts weather(const std::string& cost, const std::string& mapExtra = "", const std::string& events = "") {
  Opts o;
  o.terrain = "{\"id\":\"plain\"},{\"id\":\"swamp\",\"moveCost\":{\"foot\":2,\"wheel\":3}" + (cost.empty() ? std::string() : ",\"weatherCost\":" + cost) + "}";
  o.mapExtra = mapExtra;
  o.events = events;
  return o;
}

const char* kCost = "{\"rain\":{\"foot\":4},\"snow\":{\"foot\":1,\"wheel\":2}}";

}  // namespace

TEST(fid2_weather_cost_loader_refusals) {
  CHECK(says(error(make("w_r1", weather("{\"fog\":{\"foot\":2}}"))), "unknown weather \"fog\""));
  CHECK(says(error(make("w_r2", weather("{\"rain\":{\"boat\":2}}"))), "unknown move type \"boat\""));
  CHECK(says(error(make("w_r3", weather("{\"rain\":{\"foot\":0}}"))), "from 1 to 99"));
  CHECK(says(error(make("w_r4", weather("{\"rain\":{\"foot\":2.5}}"))), "from 1 to 99"));
  CHECK(says(error(make("w_r5", weather("{\"rain\":{\"foot\":100}}"))), "from 1 to 99"));
  CHECK(says(error(make("w_r6", weather("{\"rain\":{}}"))), "must be { \"<move type>\": cost }"));
  CHECK(says(error(make("w_r7", weather("[]"))), "\"weatherCost\" must be"));
  CHECK(says(error(make("w_r8", weather(kCost, "\"weather\": \"hail\","))), "\"weather\" must be \"rain\", \"snow\" or \"none\""));
  CHECK_STR(error(make("w_ok", weather(kCost, "\"weather\": \"none\","))), "");
}

TEST(fid2_weather_changes_what_a_terrain_costs) {
  World w(make("w_cost", weather(kCost, "\"weather\": \"rain\",")));
  const Cell swamp = w.at(3, 1);
  CHECK_EQ(w.b().enterCost(swamp, 0), 4);  // rain: the map starts with it
  CHECK_EQ(w.b().enterCost(swamp, 1), 3);  // the wheel has no rain entry: its own cost
  CHECK_EQ(w.b().enterCost(w.at(0, 0), 0), 1);  // plain has no weatherCost
  w.b().storyState.weather = "snow";
  CHECK_EQ(w.b().enterCost(swamp, 0), 1);
  CHECK_EQ(w.b().enterCost(swamp, 1), 2);
  w.b().storyState.weather.clear();
  CHECK_EQ(w.b().enterCost(swamp, 0), 2);  // clear sky: the terrain's own number
  // A game with no weatherCost anywhere never looks at the weather.
  World plain(make("w_plain", weather("", "\"weather\": \"rain\",")));
  CHECK(!plain.db().anyWeatherCost);
  CHECK_EQ(plain.b().enterCost(plain.at(3, 1), 0), 2);
}

TEST(fid2_weather_cost_reaches_movement_range_and_follows_the_story) {
  // hero (move 4) reaches the swamp at 3,1 over 1,0 2,0 3,0 only when it costs 1 (3 + 1).
  const std::string events = "{\"id\":\"snow_now\",\"when\":{\"turn\":2},\"do\":[{\"weather\":\"snow\"}]},"
                             "{\"id\":\"clear_now\",\"when\":{\"turn\":3},\"do\":[{\"weather\":\"none\"}]}";
  World w(make("w_reach", weather(kCost, "\"weather\": \"rain\",", events)));
  Reach r;
  w.b().reachable(w.id("hero"), r);
  CHECK_EQ(r.cost[(size_t)w.at(3, 1)], -1);  // rain: 3 + 4 = 7
  w.round();  // round 2 begins: the story sets snow
  CHECK_STR(w.b().storyState.weather, "snow");
  w.b().reachable(w.id("hero"), r);
  CHECK_EQ(r.cost[(size_t)w.at(3, 1)], 4);  // snow: 3 + 1
  w.round();  // round 3: clear
  CHECK_STR(w.b().storyState.weather, "");
  w.b().reachable(w.id("hero"), r);
  CHECK_EQ(r.cost[(size_t)w.at(3, 1)], -1);  // 3 + 2 = 5 > 4
}

TEST(fid2_weather_is_saved_only_when_set) {
  const std::string events = "{\"id\":\"rain_now\",\"when\":{\"battle\":\"start\"},\"do\":[{\"weather\":\"rain\"}]}";
  World w(make("w_save", weather(kCost, "", events)));
  CHECK_STR(w.b().storyState.weather, "rain");
  Json j;
  saveBattleState(w.b(), j);
  CHECK(j.dump(0).find("\"weather\":\"rain\"") != std::string::npos);
  World fresh(make("w_save2", weather(kCost, "", events)), false);  // initialised, not started: clear
  CHECK_STR(fresh.b().storyState.weather, "");
  std::string err;
  CHECK(restoreBattleState(fresh.b(), j, err));
  CHECK_STR(fresh.b().storyState.weather, "rain");
  CHECK_EQ(fresh.b().enterCost(fresh.at(3, 1), 0), 4);
  // A battle that never had weather writes nothing about it.
  World none(make("w_save3", weather(kCost, "", "{\"id\":\"x\",\"when\":{\"battle\":\"start\"},\"do\":[{\"say\":\"hi\"}]}")));
  Json k;
  saveBattleState(none.b(), k);
  CHECK(k.dump(0).find("weather") == std::string::npos);
}
