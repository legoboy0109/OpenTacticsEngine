// Changing the map during a battle (docs/EVENTS.md "Changing the map"): the loader, setTerrain / applyChange / revertChange, what
// derives from terrain, the stranded-unit rule, events, saves and hexagons. Game: tests/fixtures/mapchange.
#include <cstring>

#include "core/ai/ai_internal.h"
#include "mapchange_fixture.h"

using namespace ote;
using namespace mctest;

namespace {

// A copy of the fixture whose bridge map has `changes` replaced by the given text.
std::string badChange(const char* name, const char* changes) {
  MCopy c(name);
  std::string text = c.read("maps/bridge.json");
  const size_t at = text.find("\"changes\"");
  const size_t end = text.find("\"events\"");
  c.put("maps/bridge.json", text.substr(0, at) + "\"changes\": " + changes + ",\n  " + text.substr(end));
  return c.loadError("bridge");
}

bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

std::string badCommand(const char* name, const char* command) {
  MCopy c(name);
  c.swap("maps/bridge.json", "{ \"applyChange\": \"bridge\" }", command);
  return c.loadError("bridge");
}

}  // namespace

// ---- loader ----

TEST(mapchange_the_fixture_loads_with_its_changes) {
  MCopy c("mc_load");
  CHECK_STR(c.loadError("bridge"), "");
  Database db;
  MapDef m;
  std::string err;
  CHECK(loadGame(c.dir, db, err) && loadMap(db, "bridge", m, err));
  CHECK_EQ((int)m.changes.size(), 3);
  CHECK_STR(m.changes[0].id, "bridge");
  CHECK_EQ((int)m.changes[0].cells.size(), 1);
  CHECK_EQ(m.changes[0].cells[0].terrain, db.findTerrain("bridge"));
}

TEST(mapchange_loader_refuses_a_bad_changes_block) {
  CHECK(has(badChange("mc_l1", "[]"), "changes: must be an object"));
  CHECK(has(badChange("mc_l2", "{ \"a b\": { \"cells\": [] } }"), "letters, digits"));
  CHECK(has(badChange("mc_l3", "{ \"x\": 5 }"), "change \"x\": must be an object"));
  CHECK(has(badChange("mc_l4", "{ \"x\": { \"cells\": [ { \"x\": 1, \"y\": 1, \"terrain\": \"plain\" } ], \"oops\": 1 } }"), "unknown key \"oops\""));
  CHECK(has(badChange("mc_l5", "{ \"x\": {} }"), "changes nothing"));
  CHECK(has(badChange("mc_l6", "{ \"x\": { \"cells\": [ { \"x\": 9, \"y\": 1, \"terrain\": \"plain\" } ] } }"), "is outside the map"));
  CHECK(has(badChange("mc_l7", "{ \"x\": { \"cells\": [ { \"x\": 1, \"y\": 1, \"terrain\": \"lava\" } ] } }"), "unknown terrain \"lava\""));
  CHECK(has(badChange("mc_l8", "{ \"x\": { \"cells\": [ { \"x\": 1, \"y\": 1, \"terrain\": \"plain\" }, { \"x\": 1, \"y\": 1, \"terrain\": \"wall\" } ] } }"),
            "in this change twice"));
  CHECK(has(badChange("mc_l9", "{ \"x\": { \"cells\": [ { \"x\": 1, \"y\": 1 } ] } }"), "terrain"));
  CHECK(has(badChange("mc_l10", "{ \"x\": { \"cells\": [ { \"x\": 1, \"y\": 1, \"terrain\": \"plain\", \"height\": 2 } ] } }"), "unknown key \"height\""));
  CHECK(has(badChange("mc_l11", "{ \"x\": { \"structures\": [ { \"id\": \"nobody\", \"kind\": \"portcullis_up\" } ] } }"), "not the \"id\" of a structure"));
}

TEST(mapchange_loader_refuses_a_structure_of_another_size_and_checks_the_commands) {
  MCopy c("mc_size");
  c.swap("maps/cracked.json", "\"cells\": [ { \"x\": 3, \"y\": 2, \"terrain\": \"rubble\" }", "\"structures\": [ { \"id\": \"w1\", \"kind\": \"portcullis_up\" } ], \"cells\": [ { \"x\": 3, \"y\": 2, \"terrain\": \"rubble\" }");
  const std::string e1 = c.loadError("cracked");
  if (!has(e1, "does not have the size of")) std::printf("  got: %s\n", e1.c_str());
  CHECK(has(e1, "does not have the size of"));
  CHECK(has(badCommand("mc_c1", "{ \"applyChange\": \"nope\" }"), "which is not one of this map's \"changes\""));
  CHECK(has(badCommand("mc_c2", "{ \"applyChange\": 4 }"), "must be the id of one of the map's"));
  CHECK(has(badCommand("mc_c3", "{ \"revertChange\": \"nope\" }"), "\"revertChange\" names \"nope\""));
  CHECK(has(badCommand("mc_c4", "{ \"setTerrain\": \"lava\", \"area\": { \"x\": 1, \"y\": 1 } }"), "unknown terrain \"lava\""));
  CHECK(has(badCommand("mc_c5", "{ \"setTerrain\": \"plain\" }"), "as \"area\""));
  CHECK(has(badCommand("mc_c6", "{ \"setTerrain\": \"plain\", \"area\": { \"x\": 1, \"y\": 1 }, \"cells\": [[1, 1]] }"), "(one of them)"));
  CHECK(has(badCommand("mc_c7", "{ \"setTerrain\": \"plain\", \"area\": { \"x\": 8, \"y\": 1, \"w\": 3 } }"), "does not fit inside the map"));
  CHECK(has(badCommand("mc_c8", "{ \"setTerrain\": \"plain\", \"cells\": [[1, 9]] }"), "is outside the map"));
  CHECK(has(badCommand("mc_c9", "{ \"setTerrain\": \"plain\", \"cells\": [] }"), "must be a list of squares"));
  CHECK(has(badCommand("mc_c10", "{ \"setFog\": \"yes\" }"), "write { \"setFog\": true }"));
  CHECK(has(badCommand("mc_c11", "{ \"setVision\": 11 }"), "from -10 to 10"));
  CHECK(has(badCommand("mc_c12", "{ \"setVision\": 1.5 }"), "whole number"));
  CHECK(has(badCommand("mc_c13", "{ \"setFog\": true, \"setVision\": 1 }"), "has two commands"));
  CHECK(has(badCommand("mc_c14", "{ \"applyChange\": \"bridge\", \"extra\": 1 }"), "unknown key"));
  CHECK_STR(badCommand("mc_c15", "{ \"setVision\": -3 }"), "");
}

// ---- effects ----

TEST(mapchange_apply_a_change_makes_a_path_and_revert_takes_it_back) {
  World w("mc_apply");
  CHECK_STR(w.terrainAtXY(4, 1), "river");
  CHECK(!w.canReach("hero", 6, 1));
  CHECK(w.cost("hero", 4, 1) <= 0);
  std::vector<Event> ev;
  w.b().storyApplyChange(w.change("bridge"), ev);
  CHECK_STR(w.terrainAtXY(4, 1), "bridge");
  CHECK(w.cost("hero", 4, 1) > 0);
  CHECK(w.b().changeApplied(w.change("bridge")));
  CHECK_EQ((int)ev.size(), 1);
  CHECK(ev[0].type == EventType::TerrainChanged);
  CHECK_EQ(ev[0].a, 1);
  CHECK_EQ(ev[0].b, w.terrainId("bridge"));
  CHECK_EQ((int)ev[0].data.size(), 1);
  CHECK_EQ(ev[0].data[0], (int)w.at(4, 1));
  w.put("hero", 3, 1);
  CHECK(w.canReach("hero", 5, 1));  // through the bridge
  // Applying again does nothing.
  ev.clear();
  w.b().storyApplyChange(w.change("bridge"), ev);
  CHECK(ev.empty());
  // Revert restores the river.
  w.put("hero", 1, 1);
  w.b().storyRevertChange(w.change("bridge"), ev);
  CHECK_STR(w.terrainAtXY(4, 1), "river");
  CHECK(!w.b().changeApplied(w.change("bridge")));
  CHECK_EQ((int)ev.size(), 1);
  CHECK(ev[0].type == EventType::TerrainChanged);
  // Reverting something that is not applied does nothing.
  ev.clear();
  w.b().storyRevertChange(w.change("bridge"), ev);
  CHECK(ev.empty());
}

TEST(mapchange_revert_restores_what_the_squares_held_when_the_change_was_applied) {
  World w("mc_revert_before");
  std::vector<Event> ev;
  // Another change touches the same square first; reverting "ford" puts back what ford replaced (the first one's bridge).
  w.b().storyApplyChange(w.change("bridge"), ev);
  w.b().storyApplyChange(w.change("ford"), ev);
  CHECK_STR(w.terrainAtXY(4, 1), "rubble");
  CHECK_STR(w.terrainAtXY(4, 0), "bridge");
  w.b().storyRevertChange(w.change("ford"), ev);
  CHECK_STR(w.terrainAtXY(4, 1), "bridge");
  CHECK_STR(w.terrainAtXY(4, 0), "river");
}

TEST(mapchange_set_terrain_by_area_and_by_cells_and_a_no_op_says_nothing) {
  World w("mc_setterrain");
  std::vector<Event> ev;
  std::vector<Cell> sq;
  for (int y = 0; y < 3; ++y) sq.push_back(w.at(4, y));
  w.b().storySetTerrain(sq, w.terrainId("bridge"), ev);
  CHECK_EQ((int)ev.size(), 1);
  CHECK_EQ(ev[0].a, 3);
  CHECK_EQ(ev[0].b, w.terrainId("bridge"));
  CHECK_STR(w.terrainAtXY(4, 2), "bridge");
  ev.clear();
  w.b().storySetTerrain(sq, w.terrainId("bridge"), ev);  // already so
  CHECK(ev.empty());
  w.b().storySetTerrain(std::vector<Cell>{w.at(4, 0), w.at(0, 0)}, w.terrainId("wall"), ev);
  CHECK_EQ(ev[0].a, 2);
  CHECK_EQ(ev[0].b, w.terrainId("wall"));
  w.b().storySetTerrain(std::vector<Cell>{w.at(4, 0), w.at(0, 1)}, w.terrainId("rubble"), ev);  // mixed result in one event is -1
  w.b().storySetTerrain(std::vector<Cell>{w.at(0, 0), w.at(0, 1)}, w.terrainId("river"), ev);
  CHECK_STR(w.terrainAtXY(0, 0), "river");
  // setTerrain never touches heights.
  CHECK_EQ(w.b().cells[(size_t)w.at(0, 0)].height, 0);
}

TEST(mapchange_the_story_commands_run_from_events) {
  World w("mc_events");
  CHECK_STR(w.terrainAtXY(4, 1), "river");
  CHECK(!w.sw("bridged"));
  w.redTurn();  // round 1 ends, round 2 begins: the turn 2 event applies the bridge
  CHECK_STR(w.terrainAtXY(4, 1), "bridge");
  CHECK(w.sw("bridged"));
  CHECK(w.b().changeApplied(w.change("bridge")));
}

TEST(mapchange_terrain_derived_data_follows_at_once_movement_costs_and_numbers) {
  World w("mc_derived");
  const Cell c = w.at(4, 1);
  CHECK(w.b().enterCost(c, w.b().typeOf(w.unit("hero")).moveType) <= 0);
  CHECK(w.b().enterCost(c, w.b().typeOf(w.unit("wing")).moveType) > 0);  // a flyer crosses a river anyway
  std::vector<Event> ev;
  w.b().storyApplyChange(w.change("ford"), ev);  // 4,1 becomes rubble: cost 2 on foot
  CHECK_EQ(w.b().enterCost(c, w.b().typeOf(w.unit("hero")).moveType), 2);
  CHECK_STR(w.b().terrainAt(c).id.c_str(), "rubble");
}

TEST(mapchange_the_ai_crosses_a_bridge_that_appears) {
  World w("mc_ai");
  // No way across: the red raider can only wait on its side. Blue ends turns; red keeps to x >= 5.
  for (int t = 0; t < 1; ++t) w.redTurn();
  CHECK(w.unit("r1").cell != kNoCell);
  CHECK(w.b().grid.x(w.unit("r1").cell) >= 5 || w.b().over);
  // The bridge event (round 2) opens a way; within a few turns red is across or fights.
  w.put("hero", 3, 1);
  w.put("wing", 3, 0);
  int crossed = 0;
  for (int t = 0; t < 4 && !w.b().over; ++t) {
    w.redTurn();
    if (w.unit("r1").alive && w.b().grid.x(w.unit("r1").cell) <= 4) crossed = 1;
  }
  CHECK(crossed || w.b().over);
  CHECK(w.b().changeApplied(w.change("bridge")));
}

TEST(mapchange_ai_distance_field_sees_the_new_path) {
  World w("mc_field");
  const int mt = w.b().typeOf(w.unit("r1")).moveType;
  std::vector<int> field;
  ai::distanceField(w.b(), mt, std::vector<Cell>(1, w.unit("hero").cell), field);
  CHECK(field[(size_t)w.unit("r1").cell] >= ai::kFar);
  std::vector<Event> ev;
  w.b().storyApplyChange(w.change("bridge"), ev);
  ai::distanceField(w.b(), mt, std::vector<Cell>(1, w.unit("hero").cell), field);
  CHECK(field[(size_t)w.unit("r1").cell] < ai::kFar);
  w.b().storyRevertChange(w.change("bridge"), ev);
  ai::distanceField(w.b(), mt, std::vector<Cell>(1, w.unit("hero").cell), field);
  CHECK(field[(size_t)w.unit("r1").cell] >= ai::kFar);
}

// ---- the stranded-unit rule ----

TEST(mapchange_a_unit_that_cannot_stand_on_the_new_terrain_is_moved_to_the_nearest_square_it_can) {
  World w("mc_strand");
  w.put("hero", 2, 1);
  std::vector<Event> ev;
  w.b().storyApplyChange(w.change("sink"), ev);  // 2,1 and 2,2 become river
  CHECK_STR(w.terrainAtXY(2, 1), "river");
  // Hero (foot) is moved: the nearest free standable square to 2,1 (lowest number on a tie): 1,1 (distance 1) is free now.
  const Cell now = w.unit("hero").cell;
  CHECK(now != w.at(2, 1));
  CHECK_EQ(w.b().grid.distance(now, w.at(2, 1)), 1);
  CHECK(w.cost("hero", w.b().grid.x(now), w.b().grid.y(now)) > 0);
  CHECK_EQ(w.b().occupant[(size_t)now], w.id("hero"));
  CHECK_EQ(w.b().occupant[(size_t)w.at(2, 1)], -1);
  // Events: the terrain first, then the relocation.
  CHECK(ev.size() >= 2 && ev[0].type == EventType::TerrainChanged && ev[1].type == EventType::UnitRelocated);
  CHECK_EQ(ev[1].a, w.id("hero"));
  CHECK_EQ(ev[1].b, (int)w.at(2, 1));
  CHECK_EQ(ev[1].c, (int)now);
}

TEST(mapchange_a_flyer_and_a_unit_that_can_still_stand_stay_put) {
  World w("mc_strand_fly");
  w.put("wing", 2, 1);
  std::vector<Event> ev;
  w.b().storyApplyChange(w.change("sink"), ev);
  CHECK_EQ(w.unit("wing").cell, w.at(2, 1));  // a river is fine for a flyer
  CHECK_EQ((int)ev.size(), 1);
  // Terrain that is not harder to stand on: nobody moves.
  w.put("hero", 4, 1);  // (the bridge square is a river now: put by hand, the rules are not asked)
  std::vector<Event> ev2;
  w.b().storyApplyChange(w.change("bridge"), ev2);
  CHECK_EQ(w.unit("hero").cell, w.at(4, 1));
  CHECK_EQ((int)ev2.size(), 1);
}

TEST(mapchange_a_stranded_unit_with_no_room_anywhere_stays) {
  // A map of two squares where every other square is taken: nowhere to go.
  World w("mc_strand_full", "bridge", [](MCopy& c) {
    c.swap("maps/bridge.json", "\"width\": 9, \"height\": 3", "\"width\": 2, \"height\": 1");
    c.swap("maps/bridge.json", "[ \"....~....\", \"....~....\", \"....~....\" ]", "[ \"..\" ]");
    c.put("maps/bridge.json", "{ \"name\": \"Tiny\", \"grid\": { \"type\": \"square\", \"width\": 2, \"height\": 1 }, \"legend\": { \".\": \"plain\" }, \"tiles\": [ \"..\" ],"
          " \"sides\": [ { \"id\": \"blue\", \"controller\": \"human\" }, { \"id\": \"red\", \"controller\": \"ai\" } ],"
          " \"units\": [ { \"type\": \"fighter\", \"id\": \"hero\", \"side\": \"blue\", \"x\": 0, \"y\": 0, \"items\": [\"iron_sword\"] },"
          " { \"type\": \"raider\", \"id\": \"r1\", \"side\": \"red\", \"x\": 1, \"y\": 0, \"items\": [\"iron_sword\"] } ],"
          " \"changes\": { \"flood\": { \"cells\": [ { \"x\": 0, \"y\": 0, \"terrain\": \"river\" } ] } } }");
  });
  std::vector<Event> ev;
  w.b().storyApplyChange(w.change("flood"), ev);
  CHECK_EQ(w.unit("hero").cell, w.at(0, 0));  // nowhere to go: it stays (and can move off)
  CHECK_EQ((int)ev.size(), 1);
}

// ---- structures in a change ----

TEST(mapchange_a_change_turns_a_structure_into_another_kind_and_back) {
  World w("mc_kind", "dark");
  CHECK_STR(w.kind("gate"), "portcullis");
  std::vector<Event> ev;
  w.b().storyApplyChange(w.change("gap"), ev);
  CHECK_STR(w.kind("gate"), "portcullis_up");
  CHECK_STR(w.terrainAtXY(5, 1), "plain");
  CHECK(ev.size() >= 2);
  CHECK(ev[0].type == EventType::StructureUpgraded);
  CHECK_EQ(ev[0].a, w.place("gate"));
  CHECK(ev[1].type == EventType::TerrainChanged);
  w.b().storyRevertChange(w.change("gap"), ev);
  CHECK_STR(w.kind("gate"), "portcullis");
  CHECK_STR(w.terrainAtXY(5, 1), "thicket");
}

// ---- saves ----

TEST(mapchange_a_battle_with_no_change_saves_exactly_as_before) {
  World w("mc_save_plain");
  const std::string text = savetest::stateText(w.b());
  CHECK(!has(text, "mapState"));
  World f("mc_save_plain_fog", "dark");
  CHECK(!has(savetest::stateText(f.b()), "mapState"));
}

TEST(mapchange_a_save_mid_battle_with_an_applied_change_comes_back_the_same) {
  World w("mc_save_round");
  std::vector<Event> ev;
  w.b().storyApplyChange(w.change("ford"), ev);
  w.b().storySetTerrain(std::vector<Cell>{w.at(0, 2)}, w.terrainId("wall"), ev);
  const std::string saved = savetest::stateText(w.b());
  CHECK(has(saved, "\"mapState\""));
  CHECK(has(saved, "\"ford\""));
  // A fresh battle from the same map, restored from the save.
  World g("mc_save_round_b", "bridge", nullptr, false);
  Json j;
  std::string err;
  CHECK(Json::parse(saved, j, err));
  CHECK(restoreBattleState(g.b(), j, err));
  if (!err.empty()) std::printf("  %s\n", err.c_str());
  CHECK_STR(g.terrainAtXY(4, 0), "bridge");
  CHECK_STR(g.terrainAtXY(4, 1), "rubble");
  CHECK_STR(g.terrainAtXY(0, 2), "wall");
  CHECK(g.b().changeApplied(g.change("ford")));
  CHECK(savetest::stateText(g.b()) == saved);
  // And the revert still knows what to put back.
  std::vector<Event> ev2;
  g.b().storyRevertChange(g.change("ford"), ev2);
  CHECK_STR(g.terrainAtXY(4, 1), "river");
  CHECK_STR(g.terrainAtXY(4, 0), "river");
  CHECK_STR(g.terrainAtXY(0, 2), "wall");  // the setTerrain is not part of the change
}

TEST(mapchange_a_save_with_a_structure_kind_swap_round_trips) {
  World w("mc_save_kind", "dark");
  std::vector<Event> ev;
  w.b().storyApplyChange(w.change("gap"), ev);
  const std::string saved = savetest::stateText(w.b());
  World g("mc_save_kind_b", "dark", nullptr, false);
  Json j;
  std::string err;
  CHECK(Json::parse(saved, j, err));
  CHECK(restoreBattleState(g.b(), j, err));
  if (!err.empty()) std::printf("  %s\n", err.c_str());
  CHECK_STR(g.kind("gate"), "portcullis_up");
  CHECK(savetest::stateText(g.b()) == saved);
  g.b().storyRevertChange(g.change("gap"), ev);
  CHECK_STR(g.kind("gate"), "portcullis");
}

TEST(mapchange_a_damaged_save_is_refused_with_a_path) {
  World w("mc_save_bad");
  std::vector<Event> ev;
  w.b().storyApplyChange(w.change("bridge"), ev);
  const std::string saved = savetest::stateText(w.b());
  auto refused = [&](const char* from, const char* to, const char* expect) {
    std::string text = saved;
    const size_t at = text.find(from);
    if (at == std::string::npos) { std::printf("  missing %s\n", from); CHECK(false); return; }
    text.replace(at, strlen(from), to);
    World g("mc_save_bad_b", "bridge", nullptr, false);
    Json j;
    std::string err;
    if (!Json::parse(text, j, err)) { std::printf("  parse %s\n", err.c_str()); CHECK(false); return; }
    CHECK(!restoreBattleState(g.b(), j, err));
    if (!has(err, expect)) std::printf("  got: %s\n", err.c_str());
    CHECK(has(err, expect));
  };
  refused("\"change\":\"bridge\"", "\"change\":\"nope\"", "mapState.applied[0].change");
  refused("\"terrain\":[[13,3]]", "\"terrain\":[[13,99]]", "mapState.terrain[0]");
  refused("\"terrain\":[[13,3]]", "\"terrain\":[[999,3]]", "mapState.terrain[0]");
  refused("\"mapState\":{", "\"mapState\":{\"fog\":true,\"zzz\":1,", "unknown key");
}

// ---- hexagons ----

TEST(mapchange_works_on_a_hexagon_grid) {
  World w("mc_hex", "bridge", [](MCopy& c) {
    c.swap("game.json", "\"view\":", "\"grid\": { \"type\": \"hex\", \"layout\": \"pointy\" }, \"view\":");
    c.swap("maps/bridge.json", "\"type\": \"square\"", "\"type\": \"hex\"");
    c.put("data/structures.json", "{ \"structures\": [] }");  // the fixture's wide structures are for square games
  });
  CHECK(w.b().grid.isHex());
  CHECK(w.cost("hero", 4, 1) <= 0);
  std::vector<Event> ev;
  w.b().storyApplyChange(w.change("bridge"), ev);
  CHECK(w.cost("hero", 4, 1) > 0);
  CHECK(ev.size() == 1 && ev[0].type == EventType::TerrainChanged);
  // A stranded unit on hexagons goes to a neighbour.
  w.put("hero", 2, 1);
  w.b().storyApplyChange(w.change("sink"), ev);
  CHECK(w.b().grid.distance(w.unit("hero").cell, w.at(2, 1)) == 1);
  w.b().storyRevertChange(w.change("sink"), ev);
  CHECK_STR(w.terrainAtXY(2, 1), "plain");
}
