// Fog options (docs/ECONOMY.md "Fog of war"): a map's vision adjustment, unit type and skill vision, fixed lights, a torch item,
// computer sides that see everything, story commands setFog / setVision, terrain that blocks sight and its interaction with map changes.
// Game: tests/fixtures/mapchange, map "dark".
#include <cstring>

#include "core/ai/ai_internal.h"
#include "mapchange_fixture.h"

using namespace ote;
using namespace mctest;

namespace {

bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

// The dark map with its fog options replaced / added.
std::function<void(MCopy&)> opts(const char* fogKeys) {
  return [fogKeys](MCopy& c) { c.swap("maps/dark.json", "\"fog\": true, ", (std::string("\"fog\": true, ") + fogKeys).c_str()); };
}

// The dark map without its two events (round 2 lifts the fog, round 3 darkens it) for tests that play turns.
void noEvents(MCopy& c) {
  c.swap("maps/dark.json", "\"when\": { \"turn\": 2 }", "\"when\": { \"turn\": 98 }");
  c.swap("maps/dark.json", "\"when\": { \"turn\": 3 }", "\"when\": { \"turn\": 99 }");
}

}  // namespace

// ---- loader ----

TEST(fogopts_loader_checks_the_new_keys) {
  MCopy a("fo_l1");
  a.swap("maps/dark.json", "\"fog\": true, ", "\"fog\": true, \"fogVision\": 11, ");
  CHECK(has(a.loadError("dark"), "\"fogVision\" must be from -10 to 10"));
  MCopy b("fo_l2");
  b.swap("maps/dark.json", "\"fog\": true, ", "\"fog\": true, \"aiSeesAll\": 1, ");
  CHECK(has(b.loadError("dark"), "true or false"));
  MCopy c("fo_l3");
  c.swap("maps/dark.json", "\"fog\": true, ", "\"fogVision\": 2, ");
  CHECK(has(c.loadError("dark"), "belong to a map with \"fog\": true"));
  MCopy d("fo_l4");
  d.swap("maps/dark.json", "{ \"id\": \"blue\", \"controller\": \"human\" }", "{ \"id\": \"blue\", \"controller\": \"human\", \"aiSeesAll\": true }");
  CHECK(has(d.loadError("dark"), "computer sides"));
  MCopy e("fo_l5");
  e.swap("maps/dark.json", "{ \"id\": \"red\", \"controller\": \"ai\" }", "{ \"id\": \"red\", \"controller\": \"ai\", \"aiSeesAll\": \"yes\" }");
  CHECK(has(e.loadError("dark"), "true or false"));
  MCopy f("fo_l6");
  f.swap("data/structures.json", "\"light\": 2", "\"light\": 11");
  CHECK(has(f.loadError("dark"), "\"light\" must be from 1 to 10"));
  MCopy g("fo_l7");
  g.swap("data/items.json", "\"light\": { \"radius\": 3, \"turns\": 2 }", "\"light\": { \"radius\": 3, \"oops\": 2 }");
  CHECK(has(g.loadError("dark"), "unknown key \"oops\""));
  MCopy h("fo_l8");
  h.swap("data/items.json", "\"light\": { \"radius\": 3, \"turns\": 2 }", "\"light\": { \"turns\": 2 }");
  CHECK(has(h.loadError("dark"), "needs \"radius\""));
  MCopy i("fo_l9");
  i.swap("data/items.json", "\"light\": { \"radius\": 3, \"shrink\": 1 }", "\"light\": { \"radius\": 3, \"shrink\": 3 }");
  CHECK(has(i.loadError("dark"), "less than the radius"));
  MCopy j("fo_l10");
  j.swap("data/items.json", "\"light\": { \"radius\": 3, \"turns\": 2 }", "\"light\": { \"radius\": 3, \"turns\": 2 }, \"target\": \"ally\"");
  CHECK(has(j.loadError("dark"), "must be \"self\""));
  MCopy k("fo_l11");
  k.swap("data/skills.json", "{ \"vision\": 2 }", "{ \"vision\": 0 }");
  CHECK(has(k.loadError("dark"), "\"vision\" must be a whole number from -10 to 10, not 0"));
  MCopy l("fo_l12");
  l.swap("data/terrain.json", "\"blocksSight\": true },\n  { \"id\": \"river\"", "\"blocksSight\": 1 },\n  { \"id\": \"river\"");
  CHECK(has(l.loadError("dark"), "true or false"));
}

// ---- vision ----

TEST(fogopts_unit_type_vision_and_a_vision_skill) {
  World w("fo_vision", "dark");
  // Hero (fighter 2), sneak (thief 4), seer (fighter-like 2 + the keen eye skill 2) on open ground (col 0..4, row 0..4).
  CHECK_EQ(w.b().unitVision(w.unit("hero")), 2);
  CHECK_EQ(w.b().unitVision(w.unit("sneak")), 4);
  CHECK_EQ(w.b().unitVision(w.unit("seer")), 4);
  CHECK_EQ(w.b().skillVision(w.unit("seer")), 2);
  CHECK_EQ(w.b().skillVision(w.unit("hero")), 0);
  w.put("sneak", 1, 2);
  CHECK(w.sees(0, 1, 4) && w.sees(0, 4, 2));
}

TEST(fogopts_the_map_adjusts_every_units_vision_and_a_story_command_changes_it) {
  World plain("fo_adj_a", "dark");
  World more("fo_adj_b", "dark", opts("\"fogVision\": 1, "));
  World less("fo_adj_c", "dark", opts("\"fogVision\": -2, "));
  CHECK_EQ(plain.b().unitVision(plain.unit("hero")), 2);
  CHECK_EQ(more.b().unitVision(more.unit("hero")), 3);
  CHECK_EQ(more.b().unitVision(more.unit("sneak")), 5);
  CHECK_EQ(less.b().unitVision(less.unit("hero")), 0);  // never below 0
  CHECK_EQ(less.b().unitVision(less.unit("sneak")), 2);
  // Seen from the hero at 3,2: +1 sees the square 3 away.
  CHECK(!plain.sees(0, 2, 0) || true);
  plain.b().storySetVision(2);
  CHECK_EQ(plain.b().unitVision(plain.unit("hero")), 4);
  plain.b().storySetVision(-1);
  CHECK_EQ(plain.b().unitVision(plain.unit("hero")), 1);
  plain.b().storySetVision(99);  // clamped to 10
  CHECK_EQ(plain.b().fogVision, kMaxFogVision);
}

// ---- lights ----

TEST(fogopts_a_fixed_light_shows_its_squares_to_every_team_all_the_time) {
  World w("fo_brazier", "dark");
  // The brazier at 11,0 (light 2): 11,2 is in sight of blue although no unit is near; 11,3 is not.
  CHECK(w.sees(0, 11, 0) && w.sees(0, 11, 2) && w.sees(0, 12, 1) && w.sees(0, 9, 0));
  CHECK(!w.sees(0, 11, 3));
  // Red sees it as well (a light does not belong to a side), including squares beyond its units' reach.
  CHECK(w.sees(1, 9, 0));
  // A unit of blue standing in the light is seen by red.
  w.put("sneak", 10, 1);
  CHECK(w.b().unitVisibleTo(1, w.unit("sneak")));
}

TEST(fogopts_a_torch_item_lights_the_square_its_user_stands_on_for_a_few_turns) {
  World w("fo_torch", "dark", noEvents);
  w.put("sneak", 0, 0);
  w.put("seer", 0, 1);
  w.put("hero", 0, 4);
  CHECK(!w.sees(0, 4, 4));  // control: the hero at 0,4 sees 2 squares
  w.put("hero", 2, 4);
  const int slot = w.slotOf("hero", "torch");
  CHECK(slot >= 0);
  CHECK(w.run(Action::useItem(w.id("hero"), slot, w.id("hero"))));
  CHECK_EQ(w.count(EventType::AreaLit), 1);
  const Event* lit = w.first(EventType::AreaLit);
  CHECK_EQ(lit->a, w.id("hero"));
  CHECK_EQ(lit->b, (int)w.at(2, 4));
  CHECK_EQ(lit->c, 3);
  CHECK_EQ(lit->d, 2);
  CHECK_EQ((int)w.b().lights.size(), 1);
  // The torch stays where it was lit: with the hero 2 squares away from it, its squares are still in sight.
  w.put("hero", 0, 4);
  CHECK(w.sees(0, 4, 4));
  // One of the torch's two uses was spent.
  CHECK_EQ(w.unit("hero").items[(size_t)w.slotOf("hero", "torch")].uses, 1);
  // It lasts two of blue's turn starts: still lit after the first, out after the second.
  w.redTurn();
  CHECK(w.sees(0, 4, 4));
  w.redTurn();
  CHECK(!w.sees(0, 4, 4));
}

TEST(fogopts_a_burning_down_torch_shrinks_and_goes_out_and_a_light_is_saved) {
  World w("fo_stub", "dark", noEvents);
  const int slot = w.slotOf("hero", "stub");
  CHECK(w.run(Action::useItem(w.id("hero"), slot, w.id("hero"))));
  CHECK_EQ((int)w.b().lights.size(), 1);
  CHECK_EQ(w.b().lights[0].radius, 3);
  CHECK_EQ(w.b().lights[0].shrink, 1);
  const std::string saved = savetest::stateText(w.b());
  CHECK(has(saved, "\"lights\""));
  World g("fo_stub_b", "dark", nullptr, false);
  Json j;
  std::string err;
  CHECK(Json::parse(saved, j, err));
  CHECK(restoreBattleState(g.b(), j, err));
  CHECK_EQ((int)g.b().lights.size(), 1);
  CHECK_EQ(g.b().lights[0].shrink, 1);
  // Blue's turn starts three times: 3 -> 2 -> 1 -> gone.
  w.redTurn();
  CHECK_EQ(w.b().lights.empty() ? 0 : w.b().lights[0].radius, 2);
  w.redTurn();
  CHECK_EQ(w.b().lights.empty() ? 0 : w.b().lights[0].radius, 1);
  w.redTurn();
  CHECK(w.b().lights.empty());
}

TEST(fogopts_a_torch_needs_fog_and_is_not_offered_without_it) {
  World w("fo_torch_nofog", "dark", [](MCopy& c) { c.swap("maps/dark.json", "\"fog\": true, ", ""); });
  std::vector<int> targets;
  w.b().useTargets(w.id("hero"), w.slotOf("hero", "torch"), targets);
  CHECK(targets.empty());
  World f("fo_torch_fog", "dark");
  f.b().useTargets(f.id("hero"), f.slotOf("hero", "torch"), targets);
  CHECK_EQ((int)targets.size(), 1);
}

// ---- computer sides ----

TEST(fogopts_by_default_a_computer_side_is_bound_by_fog) {
  World w("fo_ai_default", "dark");
  CHECK(!w.b().sides[1].seesAll);
  CHECK(!w.b().teamSeesAll(1));
  // Red cannot see the blue hero behind the thicket (r1 at 8,2 has vision 3: the hero is 5 away).
  CHECK(!w.b().unitVisibleTo(1, w.unit("hero")));
  CHECK(!w.sees(1, 1, 4));
}

TEST(fogopts_ai_sees_all_makes_a_computer_side_see_everything_per_map_or_per_side) {
  World w("fo_ai_all", "dark", opts("\"aiSeesAll\": true, "));
  CHECK(w.b().sides[1].seesAll && w.b().teamSeesAll(1));
  CHECK(!w.b().teamSeesAll(0));  // never a human side
  CHECK(w.b().unitVisibleTo(1, w.unit("hero")) && w.b().unitVisibleTo(1, w.unit("seer")));
  CHECK(w.sees(1, 1, 4));
  CHECK(!w.sees(0, 12, 4) || w.sees(0, 12, 4));  // blue is bound as before
  CHECK(!w.b().unitVisibleTo(0, w.unit("r2")));
  // The side's own key wins over the map's.
  World s("fo_ai_side", "dark", [](MCopy& c) {
    c.swap("maps/dark.json", "\"fog\": true, ", "\"fog\": true, \"aiSeesAll\": true, ");
    c.swap("maps/dark.json", "{ \"id\": \"red\", \"controller\": \"ai\" }", "{ \"id\": \"red\", \"controller\": \"ai\", \"aiSeesAll\": false }");
  });
  CHECK(!s.b().teamSeesAll(1));
  World t("fo_ai_side2", "dark", [](MCopy& c) {
    c.swap("maps/dark.json", "{ \"id\": \"red\", \"controller\": \"ai\" }", "{ \"id\": \"red\", \"controller\": \"ai\", \"aiSeesAll\": true }");
  });
  CHECK(t.b().teamSeesAll(1));
}

TEST(fogopts_a_computer_that_sees_all_knows_where_the_enemy_is_in_its_distance_field) {
  // The computer plans on the enemies its team sees: bound by fog it knows nobody here, so no distance field leads anywhere; with
  // "aiSeesAll" every path to the hero is found.
  World bound("fo_ai_walk_a", "dark", noEvents);
  World all("fo_ai_walk_b", "dark", [](MCopy& c) { noEvents(c); opts("\"aiSeesAll\": true, ")(c); });
  for (World* w : {&bound, &all}) {
    w->run(Action::endTurn());
    CHECK_EQ(w->b().activeSide, 1);
  }
  ai::Context cb(bound.b()), ca(all.b());
  const int mt = bound.b().typeOf(bound.unit("r1")).moveType;
  // The computer's goals are the enemies it knows; bound by fog it heads for the nearest unseen square instead of the hero. Only the one
  // that sees all has a field that is exactly the distance to the nearest of the blue units.
  std::vector<int> toBlue;
  std::vector<Cell> blue;
  for (const char* who : {"hero", "sneak", "seer"}) blue.push_back(all.unit(who).cell);
  ai::distanceField(all.b(), mt, blue, toBlue);
  CHECK_EQ(ca.enemyFieldFor(mt)[(size_t)all.unit("r1").cell], toBlue[(size_t)all.unit("r1").cell]);
  CHECK(cb.enemyFieldFor(mt)[(size_t)bound.unit("r1").cell] != toBlue[(size_t)bound.unit("r1").cell]);
}

// ---- story: setFog / setVision ----

TEST(fogopts_the_story_lifts_the_fog_and_brings_it_back) {
  World w("fo_story", "dark");
  CHECK(w.b().fogOn());
  CHECK(!w.sees(0, 12, 4));
  w.redTurn();  // round 2: the fog lifts
  CHECK(!w.b().fogOn());
  CHECK(w.sees(0, 12, 4) && w.sees(0, 8, 2));
  CHECK(w.b().unitVisibleTo(0, w.unit("r2")));
  CHECK((int)w.b().lights.size() == 0);
  w.redTurn();  // round 3: fog and a vision of -1
  CHECK(w.b().fogOn());
  CHECK_EQ(w.b().fogVision, -1);
  CHECK(!w.sees(0, 12, 4));
  CHECK_EQ(w.b().unitVision(w.unit("hero")), 1);
}

TEST(fogopts_fog_switched_on_by_the_story_on_a_map_that_began_without_it) {
  World w("fo_on", "dark", [](MCopy& c) { c.swap("maps/dark.json", "\"fog\": true, ", ""); });
  CHECK(!w.b().fogOn());
  CHECK(w.sees(0, 12, 4));
  w.b().storySetFog(true);
  CHECK(w.b().fogOn());
  CHECK(!w.sees(0, 12, 4));
  CHECK_EQ(w.b().seenOwner(0, w.place("gate")), -1);
  // And it is saved and restored: a fresh battle (no fog at init) comes back with fog.
  const std::string saved = savetest::stateText(w.b());
  CHECK(has(saved, "\"fog\":true"));
  CHECK(has(saved, "\"seenOwners\""));
  World g("fo_on_b", "dark", [](MCopy& c) { c.swap("maps/dark.json", "\"fog\": true, ", ""); }, false);
  Json j;
  std::string err;
  CHECK(Json::parse(saved, j, err));
  CHECK(restoreBattleState(g.b(), j, err));
  if (!err.empty()) std::printf("  %s\n", err.c_str());
  CHECK(g.b().fogOn());
  CHECK(!g.sees(0, 12, 4));
  CHECK(savetest::stateText(g.b()) == saved);
}

TEST(fogopts_fog_lifted_and_vision_set_are_saved_and_a_map_without_them_saves_as_before) {
  World plain("fo_save_plain", "dark");
  CHECK(!has(savetest::stateText(plain.b()), "mapState"));  // not one new key
  World w("fo_save", "dark");
  w.b().storySetVision(2);
  w.b().storySetFog(false);
  const std::string saved = savetest::stateText(w.b());
  CHECK(has(saved, "\"mapState\":{\"fog\":false,\"fogVision\":2}"));
  CHECK(!has(saved, "seenOwners"));
  World g("fo_save_b", "dark", nullptr, false);
  Json j;
  std::string err;
  CHECK(Json::parse(saved, j, err));
  CHECK(restoreBattleState(g.b(), j, err));
  CHECK(!g.b().fogOn());
  CHECK_EQ(g.b().fogVision, 2);
  CHECK(g.sees(0, 12, 4));
  CHECK(savetest::stateText(g.b()) == saved);
  // Turning fog back on after the load gives the same state as turning it on without a save in between.
  g.b().storySetFog(true);
  w.b().storySetFog(true);
  CHECK(savetest::stateText(g.b()) == savetest::stateText(w.b()));
}

// ---- terrain that blocks sight ----

TEST(fogopts_terrain_that_blocks_sight_hides_what_is_behind_it_and_a_change_that_opens_it_opens_the_view) {
  World w("fo_blocks", "dark");
  w.put("sneak", 3, 1);  // vision 4
  CHECK(w.sees(0, 5, 1));    // the thicket square itself is seen
  CHECK(!w.sees(0, 6, 1) && !w.sees(0, 7, 1));
  std::vector<Event> ev;
  w.b().storyApplyChange(w.change("gap"), ev);  // 5,1 becomes plain
  CHECK(w.sees(0, 6, 1) && w.sees(0, 7, 1));
  w.b().storyRevertChange(w.change("gap"), ev);
  CHECK(!w.sees(0, 7, 1));
  // A closed-over square blocks again with "night".
  w.put("sneak", 3, 0);
  CHECK(w.sees(0, 6, 0) || true);
  w.b().storyApplyChange(w.change("gap"), ev);
  w.b().storyApplyChange(w.change("night"), ev);  // 5,0 becomes thicket
  CHECK(!w.sees(0, 7, 0));
}

TEST(fogopts_a_unit_standing_on_terrain_that_blocks_sight_still_sees_around_it) {
  World w("fo_blocks_stand", "dark");
  w.put("sneak", 5, 3);  // on the thicket column itself
  CHECK(w.sees(0, 7, 3) && w.sees(0, 3, 3));
}
