// Fog of war: the rules (docs/ECONOMY.md "Fog of war"): targeting only what the team sees, moves planned on what it
// sees and stopped by an ambush, last seen structure owners, and that the truth still decides who wins.
#include "fog_fixture.h"

using namespace ote;
using namespace fogtest;

namespace {

// The forest hides its unit and costs 1, so a hideout does not change the movement arithmetic.
void flatHideout(FogWorld& w) {
  w.forestHides();
  w.db.terrains[(size_t)w.db.findTerrain("forest")].moveCost = {1};
}

int reachableSquares(const Reach& r) {
  int n = 0;
  for (int c : r.cost) n += c >= 0;
  return n;
}

}  // namespace

// ---- targeting ----

TEST(fog_targeting_attack_options_leave_out_units_the_team_does_not_see) {
  FogWorld w(field(8, 3), {{"archer", 0, 2, 1, false}, {"soldier", 1, 4, 1, false}});
  w.vision("archer", 1);
  CHECK(w.begin());
  std::vector<AttackOption> options;
  w.battle.attackOptions(0, w.at(2, 1), options);
  CHECK(options.empty());                                    // the bow reaches it, the archer's eyes do not
  CHECK(w.refused(Action::attack(0, 1, 1)).size() > 0);      // refused like any bad attack
  Forecast f;
  CHECK(!w.battle.forecast(0, 1, 1, w.at(2, 1), f));
  CHECK(!w.battle.forecast(0, 1, 1, w.at(3, 1), f));         // "as if" it stood nearer: still judged on present sight
}

TEST(fog_targeting_a_unit_in_sight_can_be_attacked_as_before) {
  FogWorld w(field(8, 3), {{"archer", 0, 2, 1, false}, {"soldier", 1, 4, 1, false}});
  CHECK(w.begin());
  std::vector<AttackOption> options;
  w.battle.attackOptions(0, w.at(2, 1), options);
  CHECK_EQ(options.size(), size_t(1));
  CHECK(w.battle.validate(Action::attack(0, 1, 1)));
  CHECK(w.act(Action::attack(0, 1, 1)));
  CHECK(w.battle.units[1].hp < 10);
}

TEST(fog_targeting_what_a_move_reveals_can_be_attacked_afterwards) {
  FogWorld w(field(8, 3), {{"blind", 0, 0, 1, false}, {"soldier", 1, 3, 1, false}});
  w.vision("blind", 1);
  CHECK(w.begin());
  std::vector<AttackOption> options;
  w.battle.attackOptions(0, w.at(2, 1), options);
  CHECK(options.empty());
  CHECK(w.act(Action::move(0, w.at(2, 1))));
  w.battle.attackOptions(0, w.at(2, 1), options);
  CHECK_EQ(options.size(), size_t(1));
  CHECK(w.act(Action::attack(0, 1, 0)));
}

TEST(fog_targeting_a_unit_in_a_hideout_cannot_be_attacked_from_afar) {
  FogWorld w({"........", "....f...", "........"}, {{"archer", 0, 2, 1, false}, {"soldier", 1, 4, 1, false}});
  w.forestHides();
  CHECK(w.begin());
  CHECK(w.sees(0, 4, 1));                                    // the archer sees the square (distance 2) but not the unit
  std::vector<AttackOption> options;
  w.battle.attackOptions(0, w.at(2, 1), options);
  CHECK(options.empty());
}

TEST(fog_targeting_a_counterattack_still_answers_an_attacker_nobody_sees) {
  FogWorld w(field(6, 3), {{"blind", 0, 0, 1, false}, {"soldier", 1, 1, 1, false}});
  CHECK(w.begin());
  CHECK(!w.seesUnit(0, 1));
  CHECK(w.act(Action::endTurn()));
  CHECK(w.act(Action::attack(1, 0, 0)));                     // red sees blue (vision 2)
  CHECK(w.battle.units[1].hp < 10);                          // blue countered an enemy it cannot see
}

TEST(fog_targeting_talk_is_not_offered_towards_a_unit_the_team_does_not_see) {
  FogWorld w(field(8, 3), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 7, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play(R"([{"id":"chat","when":{"talk":"eirik","to":"gorm","label":"Chat"},"do":[{"setSwitch":"saved","to":true}]}])"));
  std::vector<TalkOption> o;
  w.battle.talkOptions(0, w.at(6, 1), o);                    // as if eirik stood next to gorm
  CHECK(o.empty());                                          // but gorm is out of sight now
  CHECK(w.act(Action::move(0, w.at(3, 1))));
  CHECK(w.nextTurn());
  CHECK(w.act(Action::move(0, w.at(5, 1))));
  CHECK(w.seesUnit(0, 1));
  w.battle.talkOptions(0, w.at(6, 1), o);
  CHECK_EQ(o.size(), size_t(1));
}

// ---- movement ----

TEST(fog_movement_planning_treats_a_unit_it_does_not_see_as_absent) {
  FogWorld w(field(8, 3), {{"blind", 0, 0, 1, false}, {"soldier", 1, 1, 1, false}});
  CHECK(w.begin());
  Reach r;
  w.battle.reachable(0, r);
  CHECK(r.cost[(size_t)w.at(1, 1)] >= 0);                    // the square looks empty
  CHECK(r.canStop(w.at(1, 1)));
  CHECK(r.canStop(w.at(3, 1)));                              // and the way through it is open
  CHECK_STR(w.refused(Action::move(0, w.at(1, 1))), "");
}

TEST(fog_movement_planning_still_walks_around_a_unit_it_sees) {
  FogWorld w(field(8, 3), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 1, 1, false}});
  CHECK(w.begin());
  Reach r;
  w.battle.reachable(0, r);
  CHECK_EQ(r.cost[(size_t)w.at(1, 1)], -1);
  CHECK(!r.canStop(w.at(1, 1)));
  CHECK(w.refused(Action::move(0, w.at(1, 1))).size() > 0);
}

TEST(fog_movement_without_fog_the_enemy_is_a_wall_as_it_always_was) {
  FogWorld on(field(8, 3), {{"blind", 0, 0, 1, false}, {"soldier", 1, 1, 1, false}});
  FogWorld off(field(8, 3), {{"blind", 0, 0, 1, false}, {"soldier", 1, 1, 1, false}});
  off.map.fog = false;
  CHECK(on.begin() && off.begin());
  Reach a, b;
  on.battle.reachable(0, a);
  off.battle.reachable(0, b);
  CHECK(reachableSquares(a) > reachableSquares(b));
  CHECK(a.canStop(on.at(2, 1)) && !b.canStop(off.at(2, 1)));
  CHECK_EQ(b.cost[(size_t)off.at(1, 1)], -1);
}

TEST(fog_ambush_a_move_stops_in_front_of_an_unseen_enemy_and_ends_the_units_turn) {
  FogWorld w({"........", "..f.....", "........"}, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 2, 1, false}});
  flatHideout(w);
  CHECK(w.begin());
  CHECK(!w.seesUnit(0, 1));
  CHECK(w.act(Action::move(0, w.at(3, 1))));                 // planned through the "empty" hideout
  CHECK_EQ(w.battle.units[0].cell, w.at(1, 1));              // stopped on the last free square before it
  CHECK(w.battle.units[0].moved && w.battle.units[0].done);
  std::vector<Event> moved = w.all(EventType::UnitMoved), blocked = w.all(EventType::MoveBlocked);
  CHECK_EQ(moved.size(), size_t(1));
  CHECK_EQ(moved[0].data.size(), size_t(2));                 // (0,1) > (1,1): only the squares walked
  CHECK_EQ(moved[0].data.back(), w.at(1, 1));
  CHECK_EQ(blocked.size(), size_t(1));
  CHECK_EQ(blocked[0].a, 0);
  CHECK_EQ(blocked[0].b, 1);
  CHECK_EQ(blocked[0].c, w.at(1, 1));                        // where the unit stopped
  CHECK_EQ(blocked[0].d, w.at(2, 1));                        // where the enemy stands
  CHECK(w.seesUnit(0, 1));                                   // next to it now: seen
  CHECK(w.refused(Action::attack(0, 1, 0)).size() > 0);      // but the turn is over for this unit
  CHECK(w.refused(Action::wait(0)).size() > 0);
}

TEST(fog_ambush_the_events_come_in_the_order_moved_then_blocked) {
  FogWorld w(field(8, 3), {{"blind", 0, 0, 1, false}, {"soldier", 1, 3, 1, false}});
  CHECK(w.begin());
  w.events.clear();
  CHECK(w.act(Action::move(0, w.at(3, 1))));
  CHECK_EQ(w.events.size(), size_t(2));
  CHECK(w.events[0].type == EventType::UnitMoved && w.events[1].type == EventType::MoveBlocked);
  CHECK_EQ(w.battle.units[0].cell, w.at(2, 1));
}

TEST(fog_ambush_an_enemy_revealed_during_the_walk_still_blocks_the_plan) {
  // The unit is fast. The enemy in the hideout at (4,1) is unseen when the order is given (distance 4) but comes
  // into view when the unit reaches (2,1). The plan was made on the first picture, so it still stops.
  FogWorld w({"..........", "....f.....", ".........."}, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 4, 1, false}});
  flatHideout(w);
  w.map.units[0].stats[3] = 7;
  CHECK(w.begin());
  CHECK(!w.seesUnit(0, 1));
  CHECK(w.act(Action::move(0, w.at(7, 1))));
  CHECK_EQ(w.battle.units[0].cell, w.at(3, 1));
  CHECK_EQ(w.all(EventType::MoveBlocked).size(), size_t(1));
  CHECK(w.seesUnit(0, 1));
}

TEST(fog_ambush_blocked_at_the_very_first_step_the_unit_does_not_move_at_all) {
  FogWorld w(field(8, 3), {{"blind", 0, 0, 1, false}, {"soldier", 1, 1, 1, false}});
  CHECK(w.begin());
  w.events.clear();
  CHECK(w.act(Action::move(0, w.at(3, 1))));
  CHECK_EQ(w.battle.units[0].cell, w.at(0, 1));
  CHECK(w.battle.units[0].moved && w.battle.units[0].done);
  CHECK_EQ(w.all(EventType::UnitMoved).size(), size_t(0));   // nothing to animate
  CHECK_EQ(w.all(EventType::MoveBlocked).size(), size_t(1));
  Event b = w.all(EventType::MoveBlocked)[0];
  CHECK_EQ(b.c, w.at(0, 1));
  CHECK_EQ(b.d, w.at(1, 1));
}

TEST(fog_ambush_the_unit_never_stops_on_a_square_a_friend_stands_on) {
  FogWorld w(field(8, 3), {{"blind", 0, 0, 1, false}, {"blind", 0, 2, 1, false}, {"soldier", 1, 3, 1, false}});
  CHECK(w.begin());
  CHECK(w.act(Action::move(0, w.at(3, 1))));                 // through the friend, into the unseen enemy
  CHECK_EQ(w.battle.units[0].cell, w.at(1, 1));              // (2,1) is taken: back to the last free square
  Event b = w.all(EventType::MoveBlocked)[0];
  CHECK_EQ(b.c, w.at(1, 1));
  CHECK_EQ(b.d, w.at(3, 1));
  CHECK_EQ(b.b, 2);
}

TEST(fog_ambush_spends_fuel_only_for_the_squares_walked) {
  FogWorld w(field(8, 3), {{"blind", 0, 0, 1, false}, {"soldier", 1, 3, 1, false}});
  w.db.unitTypes[3].fuel = 10;                               // "blind" uses fuel
  CHECK(w.begin());
  CHECK_EQ(w.battle.fuel(0), 10);
  CHECK(w.act(Action::move(0, w.at(3, 1))));                 // planned: 3 squares; walked: 2
  CHECK_EQ(w.battle.units[0].cell, w.at(2, 1));
  CHECK_EQ(w.battle.fuel(0), 8);
}

TEST(fog_ambush_blocked_at_the_start_costs_no_fuel) {
  FogWorld w(field(8, 3), {{"blind", 0, 0, 1, false}, {"soldier", 1, 1, 1, false}});
  w.db.unitTypes[3].fuel = 10;
  CHECK(w.begin());
  CHECK(w.act(Action::move(0, w.at(3, 1))));
  CHECK_EQ(w.battle.fuel(0), 10);
}

TEST(fog_ambush_a_capture_in_progress_survives_when_the_unit_never_left) {
  FogWorld w({"c.......", "........", "........"}, {{"blind", 0, 0, 0, false}, {"soldier", 1, 1, 0, false}});
  w.db.rules.fCaptureAmount = "5";
  CHECK(w.begin());
  CHECK(w.act(Action::capture(0)));
  CHECK_EQ(w.structureAt(0, 0).capture, 15);
  CHECK(w.nextTurn());
  CHECK(w.act(Action::move(0, w.at(3, 0))));                 // blocked at once: it stays on the city
  CHECK_EQ(w.structureAt(0, 0).capturer, 0);
  CHECK_EQ(w.structureAt(0, 0).capture, 15);
}

TEST(fog_ambush_the_story_hears_of_where_the_unit_really_ended) {
  FogWorld w(field(8, 3), {{"blind", 0, 0, 1, false}, {"soldier", 1, 3, 1, false}});
  CHECK(w.play(R"([{"id":"near","when":{"reach":{"x":2,"y":1},"by":{"side":"blue"}},"do":[{"setSwitch":"alarm","to":true}]},
                   {"id":"far","when":{"reach":{"x":3,"y":1},"by":{"side":"blue"}},"do":[{"setSwitch":"saved","to":true}]}])"));
  CHECK(w.act(Action::move(0, w.at(3, 1))));
  CHECK(w.sw("alarm"));                                      // it stopped on (2,1): that counts as arriving
  CHECK(!w.sw("saved"));
}

TEST(fog_ambush_nothing_in_the_way_a_move_is_the_same_as_without_fog) {
  FogWorld w(field(8, 3), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 7, 2, false}});
  CHECK(w.begin());
  w.events.clear();
  CHECK(w.act(Action::move(0, w.at(3, 1))));
  CHECK_EQ(w.battle.units[0].cell, w.at(3, 1));
  CHECK(!w.battle.units[0].done);
  CHECK_EQ(w.all(EventType::MoveBlocked).size(), size_t(0));
  CHECK_EQ(w.all(EventType::UnitMoved)[0].data.size(), size_t(4));
}

// ---- structures the team last saw ----

TEST(fog_owner_the_map_starting_owners_are_known_to_everybody) {
  FogWorld w({"c.......c", ".........", "........."}, {{"soldier", 0, 3, 1, false}, {"soldier", 1, 5, 1, false}});
  w.own(0, 0, 0);
  w.own(8, 0, 1);
  CHECK(w.begin());
  CHECK(!w.battle.structureVisibleTo(0, w.structure(8, 0)));
  CHECK_EQ(w.battle.seenOwner(0, w.structure(8, 0)), 1);
  CHECK_EQ(w.battle.seenOwner(1, w.structure(0, 0)), 0);
  CHECK_EQ(w.battle.seenOwnerForSide(1, w.structure(8, 0)), 1);
  CHECK_EQ(w.battle.seenOwner(0, -1), -1);
  CHECK_EQ(w.battle.seenOwner(9, 0), -1);
}

TEST(fog_owner_goes_stale_out_of_sight_and_refreshes_when_seen_again) {
  FogWorld w({"..c.....", "........", "........"}, {{"soldier", 0, 1, 1, false}, {"soldier", 1, 7, 2, false}});
  w.own(2, 0, 1);
  CHECK(w.begin());
  const int city = w.structure(2, 0);
  CHECK(w.battle.structureVisibleTo(0, city));
  CHECK_EQ(w.battle.seenOwner(0, city), 1);
  w.battle.structures[(size_t)city].owner = 0;               // the truth changes while blue watches ...
  w.battle.refreshSight();
  CHECK_EQ(w.battle.seenOwner(0, city), 0);                  // ... and blue knows
  CHECK(w.act(Action::move(0, w.at(3, 2))));
  w.battle.structures[(size_t)city].owner = 1;               // red takes it back while blue's unit is away
  w.battle.refreshSight();
  CHECK(!w.battle.structureVisibleTo(0, city));
  CHECK_EQ(w.battle.seenOwner(0, city), 0);                  // blue still believes it holds it
  CHECK_EQ(w.battle.structures[(size_t)city].owner, 1);      // the truth is elsewhere
  CHECK(w.nextTurn());
  CHECK(w.act(Action::move(0, w.at(2, 1))));                 // and sees it again
  CHECK_EQ(w.battle.seenOwner(0, city), 1);
}

TEST(fog_owner_a_capture_nobody_saw_stays_unknown_to_the_other_team_until_it_looks) {
  FogWorld w({"..........c.", "............", "............"}, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 10, 0, false}});
  CHECK(w.begin());
  const int city = w.structure(10, 0);
  CHECK(w.act(Action::move(0, w.at(3, 1))));
  CHECK(w.act(Action::endTurn()));                           // red captures the city it stands on
  CHECK(w.act(Action::capture(1)));
  CHECK_EQ(w.battle.structures[(size_t)city].owner, 1);
  CHECK_EQ(w.battle.seenOwner(1, city), 1);                  // red saw it happen
  CHECK_EQ(w.battle.seenOwner(0, city), -1);                 // blue did not: still nobody's
  CHECK(w.act(Action::endTurn()));
  CHECK(w.act(Action::move(0, w.at(6, 1))));
  CHECK_EQ(w.battle.seenOwner(0, city), -1);
  CHECK(w.nextTurn());
  CHECK(w.act(Action::move(0, w.at(9, 1))));                 // distance 2 from the city
  CHECK_EQ(w.battle.seenOwner(0, city), 1);
}

TEST(fog_owner_the_old_owner_always_learns_that_it_lost_a_structure) {
  FogWorld w({"...H....", "........", "........"}, {{"soldier", 0, 3, 0, false}, {"soldier", 1, 7, 2, false}});
  w.own(3, 0, 1);
  CHECK(w.begin());
  const int hq = w.structure(3, 0);
  CHECK(w.act(Action::capture(0)));
  CHECK_EQ(w.battle.seenOwner(0, hq), 0);
  CHECK_EQ(w.battle.seenOwner(1, hq), 0);                    // red lost it: the event is its own
}

TEST(fog_owner_a_defeated_sides_structures_turn_neutral_for_everybody) {
  FogWorld w({"c.........", "..........", ".........."}, {{"soldier", 0, 5, 1, false}, {"soldier", 1, 9, 1, false}});
  w.own(0, 0, 1);
  CHECK(w.begin());
  const int city = w.structure(0, 0);
  CHECK_EQ(w.battle.seenOwner(0, city), 1);
  w.battle.units[1].hp = 1;
  CHECK(w.act(Action::move(0, w.at(8, 1))));
  CHECK(w.act(Action::attack(0, 1, 0)));
  CHECK(w.battle.over);
  CHECK_EQ(w.battle.structures[(size_t)city].owner, -1);
  CHECK_EQ(w.battle.seenOwner(0, city), -1);                 // SideDefeated is public: blue never saw the city
}

// ---- the truth decides ----

TEST(fog_truth_a_battle_is_won_by_a_kill_the_winner_never_saw) {
  FogWorld w(field(6, 3), {{"blind", 0, 0, 1, false}, {"soldier", 1, 1, 1, false}});
  CHECK(w.begin());
  w.battle.units[1].hp = 1;
  CHECK(w.act(Action::endTurn()));
  CHECK(w.act(Action::attack(1, 0, 0)));                     // red hits blue; blue's counter kills red
  CHECK(w.battle.over);
  CHECK_EQ(w.battle.winnerTeam, 0);
  CHECK(!w.battle.units[1].alive);
}

TEST(fog_truth_story_events_fire_on_what_really_happened) {
  FogWorld w(field(10, 3), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 9, 1, false}}, {"hero", "gorm"});
  CHECK(w.play(R"([{"id":"watch","when":{"reach":{"x":6,"y":1},"by":{"unit":"gorm"}},"do":[{"setSwitch":"alarm","to":true}]}])"));
  CHECK(w.act(Action::endTurn()));
  CHECK(!w.seesUnit(0, 1));
  CHECK(w.act(Action::move(1, w.at(6, 1))));
  CHECK(w.sw("alarm"));                                      // blue saw nothing; the event ran anyway
}

TEST(fog_truth_objectives_see_a_unit_nobody_on_the_team_can_see) {
  FogWorld w(field(10, 3), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 9, 1, false}}, {"hero", "gorm"});
  CHECK(w.play("", R"({"lose":[{"reach":{"x":6,"y":1},"by":{"unit":"gorm"}}]})"));
  CHECK(w.act(Action::endTurn()));
  CHECK(!w.seesUnit(0, 1));
  CHECK(w.act(Action::move(1, w.at(6, 1))));
  CHECK(w.battle.over);                                      // blue loses to a unit it never saw arrive
  CHECK_EQ(w.battle.winnerTeam, 1);
}
