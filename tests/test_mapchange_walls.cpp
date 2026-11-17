// Breakable walls (docs/SRPG.md section 10 "Walls that break", docs/EVENTS.md "Changing the map"): a door kind with hit points that only
// attacks open, the "opened" place trigger as the hook that changes the map when it breaks, and the computer opponent's "walls" marker.
// Game: tests/fixtures/mapchange, map "cracked".
#include <cstring>

#include "core/ai/ai_internal.h"
#include "mapchange_fixture.h"

using namespace ote;
using namespace mctest;

namespace {

bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

// The cracked wall map without fog, with a marker on the red raider (or none).
void noFog(MCopy& c) { c.swap("maps/cracked.json", "\"fog\": true, ", ""); }
void marked(MCopy& c) {
  noFog(c);
  c.swap("maps/cracked.json", "\"x\": 4, \"y\": 0, \"items\"", "\"x\": 4, \"y\": 0, \"goesFor\": [\"walls\"], \"items\"");
}

bool playRed(World& w, int turns) {
  for (int t = 0; t < turns && !w.b().over; ++t) {
    w.redTurn();
    if (!w.b().over && w.b().activeSide != 0) return false;
  }
  return true;
}

}  // namespace

TEST(walls_a_wide_wall_that_only_attacks_open_breaks_as_one_piece_and_its_event_changes_the_map) {
  World w("walls_break", "cracked");
  const int iron = w.rig.db.findWeapon("iron_sword");
  const int s = w.place("w1");
  CHECK_STR(w.kind("w1"), "crack");
  CHECK_EQ(w.b().structures[(size_t)s].hp, 12);
  w.put("r1", 4, 1);
  // Behind the wall nothing is seen: the wall blocks sight (and the raider is on the far side).
  CHECK(!w.sees(0, 4, 1));
  PlaceForecast f;
  CHECK(w.b().placeForecast(w.id("hero"), s, iron, w.unit("hero").cell, f));
  CHECK_EQ(f.damage, 11);  // a.str 7 + might 5 - the wall's defence 1
  CHECK(!f.breaks);
  CHECK_EQ(f.hpAfter, 1);
  // Nothing opens it but attacks: no key, no unlock.
  CHECK(!w.b().placeOf(s).byKey && !w.b().placeOf(s).byTrait && w.b().placeOf(s).byBreak);
  CHECK(w.run(Action::breakPlace(w.id("hero"), s, iron)));
  CHECK_EQ(w.b().structures[(size_t)s].hp, 1);
  CHECK_STR(w.kind("w1"), "crack");
  CHECK_EQ(w.count(EventType::PlaceHit), 1);
  CHECK_EQ(w.count(EventType::TerrainChanged), 0);
  w.put("sneak", 3, 3);
  CHECK(w.run(Action::breakPlace(w.id("sneak"), w.place("w1"), iron)));
  // The wall is gone in one piece: every square of it is open at once, and the "opened" event ran its change.
  CHECK_STR(w.kind("w1"), "crack_open");
  CHECK(w.first(EventType::DoorOpened) != nullptr);
  CHECK_EQ(w.first(EventType::DoorOpened)->c, kOpenBreak);
  CHECK_EQ(w.count(EventType::TerrainChanged), 1);
  CHECK(w.sw("broken"));
  for (int x = 3; x <= 5; ++x) {
    CHECK_STR(w.terrainAtXY(x, 2), "rubble");
    CHECK_EQ(w.cost("hero", x, 2), 2);
  }
  // The view is open: the hero sees the square behind the wall that was hidden.
  CHECK(w.sees(0, 4, 1));
}

TEST(walls_the_hook_is_the_place_trigger_no_extra_key_exists) {
  // "onOpen" is not a key: a door block with it is... simply an unknown key, ignored like any other unknown structure key; the recipe is
  // the place trigger. Prove the trigger fires for every way of opening: a story openDoor does it too.
  World w("walls_hook", "cracked", [](MCopy& c) {
    c.swap("maps/cracked.json", "\"events\": [ ", "\"events\": [ { \"id\": \"e_open\", \"when\": { \"turn\": 2 }, \"do\": [ { \"openDoor\": \"w1\" } ] }, ");
  });
  CHECK(!w.sw("broken"));
  w.redTurn();  // round 2: the story opens the wall
  CHECK_STR(w.kind("w1"), "crack_open");
  CHECK(w.sw("broken"));
  CHECK_STR(w.terrainAtXY(4, 2), "rubble");
}

TEST(walls_loader_checks_the_word_and_the_marker) {
  MCopy a("walls_l1");
  a.swap("data/structures.json", "\"from\": \"next\" }", "\"from\": \"next\", \"word\": \"wall\" }");
  CHECK_STR(a.loadError("cracked"), "");
  MCopy b("walls_l2");
  b.swap("data/structures.json", "\"from\": \"next\" }", "\"from\": \"next\", \"word\": 5 }");
  CHECK(has(b.loadError("cracked"), "\"word\" must be a short word"));
  MCopy c("walls_l3");
  c.swap("data/structures.json", "\"from\": \"next\" }", "\"from\": \"next\", \"word\": \"far too long a word for a menu\" }");
  CHECK(has(c.loadError("cracked"), "\"word\" must be a short word"));
  MCopy d("walls_l4");
  marked(d);
  CHECK_STR(d.loadError("cracked"), "");
  MCopy e("walls_l5");
  e.swap("maps/cracked.json", "\"x\": 4, \"y\": 3, \"leader\": true", "\"x\": 4, \"y\": 3, \"goesFor\": [\"walls\"], \"leader\": true");
  CHECK(has(e.loadError("cracked"), "a person plays"));
  MCopy f("walls_l6");
  f.swap("maps/cracked.json", "\"x\": 4, \"y\": 0, \"items\"", "\"x\": 4, \"y\": 0, \"goesFor\": [\"gates\"], \"items\"");
  CHECK(has(f.loadError("cracked"), "not \"gates\" (or \"walls\""));
}

TEST(walls_a_marked_unit_attacks_the_wall_that_blocks_its_way_and_the_battle_goes_on) {
  World w("walls_ai", "cracked", marked);
  CHECK_EQ(w.unit("r1").aiGoals, kAiWalls);
  CHECK(playRed(w, 1));
  // It walked to the wall and hit it (10 damage: a.str 6 + might 5 - 1).
  CHECK_EQ(w.b().structures[(size_t)w.place("w1")].hp, 2);
  CHECK(playRed(w, 1));
  CHECK_STR(w.kind("w1"), "crack_open");
  CHECK(w.sw("broken"));
  CHECK_STR(w.terrainAtXY(4, 2), "rubble");  // the hook ran for the computer's attack too
  // Now the way is open: it fights instead of attacking anything else.
  CHECK(playRed(w, 3));
}

TEST(walls_by_default_the_computer_leaves_a_wall_alone) {
  World w("walls_ai_off", "cracked", noFog);
  CHECK_EQ(w.unit("r1").aiGoals, 0);
  CHECK(playRed(w, 4));
  CHECK_STR(w.kind("w1"), "crack");
  CHECK_EQ(w.b().structures[(size_t)w.place("w1")].hp, 12);
}

TEST(walls_a_marked_unit_does_not_break_a_wall_that_is_not_in_the_way) {
  // The wall row is gone: the crack stands alone in an open field, every enemy can be reached.
  World w("walls_ai_open", "cracked", [](MCopy& c) {
    marked(c);
    c.swap("maps/cracked.json", "\"www...www\"", "\".........\"");
  });
  CHECK(playRed(w, 3));
  CHECK_EQ(w.b().structures[(size_t)w.place("w1")].hp, 12);
}

TEST(walls_the_marker_is_saved_and_restored_and_a_save_without_it_is_unchanged) {
  World w("walls_save", "cracked", marked);
  const std::string saved = savetest::stateText(w.b());
  CHECK(has(saved, "\"goes\":16"));
  World g("walls_save_b", "cracked", marked, false);
  Json j;
  std::string err;
  CHECK(Json::parse(saved, j, err));
  CHECK(restoreBattleState(g.b(), j, err));
  CHECK_EQ(g.unit("r1").aiGoals, kAiWalls);
  World h("walls_save_c", "cracked", noFog);
  CHECK(!has(savetest::stateText(h.b()), "\"goes\""));
}
