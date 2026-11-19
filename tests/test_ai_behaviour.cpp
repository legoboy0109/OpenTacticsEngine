// Computer opponent: per-unit behaviour (docs/AI.md). Rules on the generated game of ai_behaviour_fixture.h, square and hexagon grids.
#include "ai_behaviour_fixture.h"
#include "core/ai/ai_internal.h"

using namespace ote;
using namespace aitest;

namespace {

std::string red(const char* id, int x, int y, const std::string& extra = "", const char* type = "soldier") {
  return std::string("{ \"type\": \"") + type + "\", \"id\": \"" + id + "\", \"side\": \"red\", \"x\": " + std::to_string(x) +
         ", \"y\": " + std::to_string(y) + (extra.empty() ? "" : ", " + extra) + " }";
}
std::string blue(const char* id, int x, int y, const char* type = "soldier") {
  return std::string("{ \"type\": \"") + type + "\", \"id\": \"" + id + "\", \"side\": \"blue\", \"x\": " + std::to_string(x) +
         ", \"y\": " + std::to_string(y) + " }";
}
std::string beh(const std::string& body) { return "\"behaviour\": " + body; }

}  // namespace

// ---- the default changes nothing ----

TEST(ai_behaviour_a_unit_without_the_key_has_the_default_behaviour_and_advances) {
  Field f("aib_default", red("r", 8, 2));
  CHECK(f.loaded);
  CHECK(f.unit("r").behaviour.isDefault());
  CHECK(!f.unit("r").awake);
  CHECK(f.redTurn());
  CHECK(f.x("r") < 8);  // it walked toward the lord
  // Writing the default out loud is the same thing.
  Field g("aib_default2", red("r", 8, 2, beh("\"advance\"")));
  CHECK(g.unit("r").behaviour.isDefault());
}

// ---- hold ----

TEST(ai_behaviour_hold_never_moves_and_attacks_only_what_is_next_to_it) {
  Field f("aib_hold", red("h", 8, 2, beh("\"hold\"")));
  CHECK(f.loaded);
  for (int i = 0; i < 3; ++i) CHECK(f.redTurn());
  CHECK_EQ(f.x("h"), 8);
  CHECK_EQ(f.hp("lord"), 20);
  f.put("lord", 7, 2);  // right next to it
  CHECK(f.redTurn());
  CHECK_EQ(f.x("h"), 8);
  CHECK(f.hp("lord") < 20);  // it hit what it could reach from its square
}

TEST(ai_behaviour_hold_uses_a_ranged_weapon_from_its_square_but_does_not_step_up_to_a_far_target) {
  Field f("aib_hold_bow", red("a", 8, 2, beh("\"hold\""), "archer"));
  f.put("lord", 6, 2);  // two squares away: in the bow's range
  CHECK(f.redTurn());
  CHECK_EQ(f.x("a"), 8);
  CHECK(f.hp("lord") < 20);
  Field g("aib_hold_far", red("a", 8, 2, beh("\"hold\""), "archer"));
  g.put("lord", 5, 2);  // three away: out of range, and it will not move to fix that
  CHECK(g.redTurn());
  CHECK_EQ(g.x("a"), 8);
  CHECK_EQ(g.hp("lord"), 20);
}

TEST(ai_behaviour_hold_still_counterattacks) {
  Field f("aib_hold_counter", red("h", 8, 2, beh("\"hold\"")));
  f.put("lord", 7, 2);
  const int before = f.hp("h");
  CHECK(f.b().apply(Action::attack(f.id("lord"), f.id("h"), f.rig.db.findWeapon("sword")), f.events));
  CHECK(f.hp("h") < before - 5);  // the lord's own hit, then ...
  CHECK(f.hp("lord") < 20);       // ... the hold unit's answer
}

// ---- guard: waits until someone is in reach ----

TEST(ai_behaviour_guard_sleeps_until_a_hostile_unit_is_within_its_move_and_weapon_reach) {
  Field f("aib_guard", red("g", 8, 2, beh("\"guard\"")), "", false, "\"x\": 1, \"y\": 2");  // distance 7: out of reach 4 + 1
  CHECK(f.loaded);
  CHECK(f.redTurn());
  CHECK_EQ(f.x("g"), 8);
  CHECK(!f.unit("g").awake);
  f.put("lord", 4, 2);  // distance 4: inside move 4 + range 1
  CHECK(f.redTurn());
  CHECK(f.unit("g").awake);
  CHECK(f.hp("lord") < 20);  // it woke and attacked in the same turn
}

TEST(ai_behaviour_a_woken_guard_acts_like_an_ordinary_unit_afterwards) {
  Field f("aib_guard_once", red("g", 8, 2, beh("\"guard\"")));
  f.put("lord", 4, 2);
  CHECK(f.redTurn());
  CHECK(f.unit("g").awake);
  f.put("lord", 0, 0);  // far away again: a woken guard goes after it
  const int x = f.x("g");
  CHECK(f.redTurn());
  CHECK(f.x("g") < x);
}

TEST(ai_behaviour_a_reactive_guard_only_hits_what_it_can_reach_and_never_approaches) {
  Field f("aib_guard_react", red("g", 8, 2, beh("{ \"mode\": \"guard\", \"wake\": \"react\" }")));
  CHECK(f.redTurn());
  CHECK_EQ(f.x("g"), 8);  // nothing in reach: it stays
  f.put("lord", 4, 2);
  CHECK(f.redTurn());
  CHECK(f.hp("lord") < 20);  // in reach: it comes and hits
  const int hit = f.x("g");
  CHECK(hit < 8);
  f.put("lord", 0, 0);       // out of reach again
  CHECK(f.redTurn());
  CHECK_EQ(f.x("g"), hit);   // it does not follow
  CHECK(!f.unit("g").awake);  // and never wakes for good
}

TEST(ai_behaviour_a_guard_that_is_hit_wakes_up_with_its_group) {
  Field f("aib_guard_hit",
          red("g1", 10, 2, beh("{ \"mode\": \"guard\", \"group\": \"pack\" }")) + ", " + red("g2", 10, 4, beh("{ \"mode\": \"guard\", \"group\": \"pack\" }")) +
              ", " + blue("arch", 8, 2, "archer"));
  CHECK(f.loaded);
  CHECK(!f.unit("g1").awake);
  CHECK(f.b().apply(Action::attack(f.id("arch"), f.id("g1"), f.rig.db.findWeapon("bow")), f.events));
  CHECK(f.unit("g1").awake);
  CHECK(f.unit("g2").awake);  // the whole group
}

// ---- groups ----

TEST(ai_behaviour_when_one_member_of_a_group_wakes_all_of_them_wake) {
  Field f("aib_group", red("g1", 5, 2, beh("{ \"mode\": \"guard\", \"group\": \"pack\" }")) + ", " +
                           red("g2", 12, 4, beh("{ \"mode\": \"guard\", \"group\": \"pack\" }")) + ", " +
                           red("solo", 12, 0, beh("\"guard\"")));
  CHECK(f.redTurn());
  CHECK(f.unit("g1").awake);   // the lord (0,2) is inside its reach
  CHECK(f.unit("g2").awake);   // far from everything, but in the group
  CHECK(!f.unit("solo").awake);  // not in the group: still asleep
  CHECK_EQ(f.x("solo"), 12);
  CHECK(f.x("g2") < 12 || f.y("g2") != 4);  // it moved
}

// ---- guardArea ----

TEST(ai_behaviour_guard_area_wakes_when_a_hostile_unit_stands_in_the_area) {
  Field f("aib_area", red("a", 9, 2, beh("{ \"mode\": \"guardArea\", \"area\": { \"x\": 4, \"y\": 1, \"w\": 2, \"h\": 3 } }")));
  CHECK(f.redTurn());  // the lord at 0,2 is nowhere near the area (and well within the unit's walking reach: still asleep)
  CHECK(!f.unit("a").awake);
  CHECK_EQ(f.x("a"), 9);
  f.put("lord", 5, 3);
  CHECK(f.redTurn());
  CHECK(f.unit("a").awake);
  CHECK(f.x("a") < 9);
}

TEST(ai_behaviour_guard_area_within_wakes_by_distance_not_by_reach) {
  Field f("aib_within", red("a", 9, 2, beh("{ \"mode\": \"guardArea\", \"within\": 3 }")), "", false, "\"x\": 4, \"y\": 2");  // distance 5
  CHECK(f.redTurn());
  CHECK(!f.unit("a").awake);  // it could reach the lord (move 4 + 1) but the lord is not within 3
  CHECK_EQ(f.x("a"), 9);
  f.put("lord", 6, 2);  // distance 3
  CHECK(f.redTurn());
  CHECK(f.unit("a").awake);
}

// ---- seek ----

TEST(ai_behaviour_seek_a_square_marches_there_and_stays) {
  Field f("aib_seek_cell", red("s", 10, 4, beh("{ \"mode\": \"seek\", \"x\": 3, \"y\": 4 }")), "", false, "\"x\": 0, \"y\": 0");
  CHECK(f.loaded);
  for (int i = 0; i < 4; ++i) CHECK(f.redTurn());
  CHECK_EQ(f.x("s"), 3);
  CHECK_EQ(f.y("s"), 4);
  CHECK(f.redTurn());
  CHECK_EQ(f.x("s"), 3);  // arrived: stays
}

TEST(ai_behaviour_seek_a_place_walks_to_the_structure) {
  Field f("aib_seek_place", red("s", 2, 4, beh("{ \"mode\": \"seek\", \"place\": \"throne\" }")), "", false, "\"x\": 0, \"y\": 0");
  for (int i = 0; i < 4; ++i) CHECK(f.redTurn());
  CHECK_EQ(f.x("s"), 12);
  CHECK_EQ(f.y("s"), 2);
}

TEST(ai_behaviour_seek_a_unit_ignores_other_targets_that_would_cost_it_ground) {
  const std::string seek = beh("{ \"mode\": \"seek\", \"unit\": \"lord\" }");
  Field f("aib_seek_unit", red("s", 6, 2, seek) + ", " + blue("grunt", 6, 4));
  CHECK(f.loaded);
  CHECK(f.redTurn());
  CHECK_EQ(f.hp("grunt"), 20);  // a free hit was on offer, but it would have cost ground
  CHECK_EQ(f.x("s"), 2);        // it went as close to the lord as it could
  // The plain unit takes the hit.
  Field g("aib_seek_unit_plain", red("s", 6, 2) + ", " + blue("grunt", 6, 4));
  CHECK(g.redTurn());
  CHECK(g.hp("grunt") < 20);
}

TEST(ai_behaviour_seek_a_unit_attacks_it_by_preference) {
  const std::string seek = beh("{ \"mode\": \"seek\", \"unit\": \"grunt\" }");
  Field f("aib_seek_pref", red("s", 3, 2, seek) + ", " + blue("grunt", 3, 4));
  CHECK(f.redTurn());
  CHECK(f.hp("grunt") < 20);
  CHECK_EQ(f.hp("lord"), 20);  // the lord was in reach too
}

TEST(ai_behaviour_a_seeker_whose_target_is_gone_goes_back_to_advancing) {
  Field f("aib_seek_gone", red("s", 8, 2, beh("{ \"mode\": \"seek\", \"unit\": \"grunt\" }")) + ", " + blue("grunt", 13, 4));
  f.unit("grunt").alive = false;
  f.b().occupant[(size_t)f.unit("grunt").cell] = -1;
  f.unit("grunt").cell = kNoCell;
  CHECK(f.redTurn());
  CHECK(f.x("s") < 8);  // toward the lord
}

// ---- start turn ----

TEST(ai_behaviour_from_turn_keeps_a_unit_idle_until_that_round) {
  Field f("aib_from", red("r", 9, 2, beh("{ \"fromTurn\": 3 }")));
  CHECK(f.redTurn());
  CHECK(f.redTurn());
  CHECK_EQ(f.x("r"), 9);  // rounds 1 and 2
  CHECK(f.redTurn());
  CHECK(f.x("r") < 9);    // round 3
}

TEST(ai_behaviour_an_idle_unit_still_answers_blows) {
  Field f("aib_from_counter", red("r", 8, 2, beh("{ \"fromTurn\": 5 }")));
  f.put("lord", 7, 2);
  CHECK(f.b().apply(Action::attack(f.id("lord"), f.id("r"), f.rig.db.findWeapon("sword")), f.events));
  CHECK(f.hp("lord") < 20);
}

// ---- conditions and the rest ----

TEST(ai_behaviour_berserk_overrides_hold) {
  Field f("aib_berserk", red("h", 8, 2, beh("\"hold\"")),
          R"({ "id": "rage", "when": { "turn": 2 }, "do": [ { "addCondition": "berserk", "to": { "unit": "h" } } ] })");
  CHECK(f.loaded);
  f.put("lord", 4, 2);
  CHECK(f.redTurn());  // round 1: it holds
  CHECK_EQ(f.x("h"), 8);
  CHECK_EQ(f.hp("lord"), 20);
  CHECK(f.redTurn());  // round 2 began with the condition: berserk walks it to the nearest unit and strikes
  CHECK(f.x("h") < 8);
  CHECK(f.hp("lord") < 20);
}

TEST(ai_behaviour_a_unit_on_a_hexagon_grid_behaves_the_same) {
  Field f("aib_hex", red("h", 8, 2, beh("\"hold\"")) + ", " + red("g", 10, 4, beh("\"guard\"")) + ", " +
                         red("s", 12, 0, beh("{ \"mode\": \"seek\", \"x\": 6, \"y\": 0 }")),
          "", true, "\"x\": 0, \"y\": 2");
  CHECK(f.loaded);
  for (int i = 0; i < 3; ++i) CHECK(f.redTurn());
  CHECK_EQ(f.x("h"), 8);
  CHECK_EQ(f.x("g"), 10);
  CHECK(!f.unit("g").awake);
  CHECK_EQ(f.x("s"), 6);
  CHECK_EQ(f.y("s"), 0);
  f.put("lord", 9, 2);
  CHECK(f.redTurn());
  CHECK(f.unit("g").awake);
}

TEST(ai_behaviour_a_favourite_target_wins_a_close_call) {
  Field f("aib_prefer", red("a", 5, 3, beh("{ \"prefer\": { \"unit\": \"lieutenant\" } }"), "archer") + ", " + blue("lieutenant", 4, 1) + ", " + blue("grunt", 6, 1));
  CHECK(f.loaded);
  CHECK(f.redTurn());
  const bool hitLieutenant = f.hp("lieutenant") < 20;
  const bool hitGrunt = f.hp("grunt") < 20;
  CHECK(hitLieutenant != hitGrunt);  // exactly one of the two was hit
  Field g("aib_prefer2", red("a", 5, 3, beh("{ \"prefer\": { \"unit\": \"grunt\" } }"), "archer") + ", " + blue("lieutenant", 4, 1) + ", " + blue("grunt", 6, 1));
  CHECK(g.redTurn());
  CHECK(g.hp("grunt") < 20);
  CHECK_EQ(g.hp("lieutenant"), 20);
  CHECK(f.hp("lieutenant") < 20);
}
