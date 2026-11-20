// Docs/SRPG.md "Action points": the refill of the pool, split moves, the soft cap, the cost and limit of attacks, the weapon / area / unit-type
// overrides, refusals with a reason, what counts as a finished unit, saves and determinism. Fixture tests/fixtures/action-points
// (numbers in ap_fixture.h).
#include "ap_fixture.h"
#include "core/battle/threat_area.h"

using namespace ote;
using aptest::Scene;
using aptest::plain;
using aptest::U;

namespace {

const std::vector<U> kDuel{{"lancer", "blue", 1, 2, "lan"}, {"dummy", "red", 8, 2, "foe"}};
const std::vector<U> kContact{{"lancer", "blue", 1, 2, "lan"}, {"dummy", "red", 2, 2, "foe"}};

// The ResourceChanged events of a side's resource in the event list, as "+20 +20".
std::string changes(const std::vector<Event>& events, int side, int resource) {
  std::string out;
  for (const Event& e : events)
    if (e.type == EventType::ResourceChanged && e.a == side && e.b == resource) out += (out.empty() ? "" : " ") + std::string(e.c >= 0 ? "+" : "") + std::to_string(e.c);
  return out;
}

}  // namespace

// ---- the pool and its refill ----

TEST(the_pool_is_filled_at_the_start_of_each_turn_and_carries_over_up_to_the_limit) {
  Scene s("ap_refill", plain(12, 5), kDuel);
  CHECK_EQ(s.ap(0), 20);  // blue's first turn
  CHECK_EQ(s.ap(1), 0);   // red has not had a turn yet
  CHECK(s.go("lan", 5, 2));  // 4 AP
  CHECK_EQ(s.ap(0), 16);
  CHECK(s.endTurn());
  CHECK_EQ(s.ap(1), 20);
  CHECK_STR(changes(s.w->events, 1, s.apIndex()), "+20");
  CHECK(s.endTurn());
  // 16 left, only 10 carry over, plus the 20 of the new turn.
  CHECK_EQ(s.ap(0), 30);
  CHECK_STR(changes(s.w->events, 0, s.apIndex()), "+14");
  CHECK(s.endTurn());
  CHECK_EQ(s.ap(1), 30);  // red kept all 20 (it spent nothing): 10 carried + 20
}

TEST(with_no_carry_over_a_turn_starts_from_the_refill_alone) {
  Scene s("ap_nocarry", plain(12, 5), kDuel, false, [](skilltest::Copy& c) { c.swap("game.json", ", \"carryOver\": 10", ""); });
  CHECK(s.go("lan", 3, 2));
  CHECK(s.endTurn());
  CHECK(s.endTurn());
  CHECK_EQ(s.ap(0), 20);
}

TEST(a_refill_works_for_any_resource_without_the_action_point_rules) {
  // The same game with the actionPoints block cut out: units move once, as always, and gold refills at the start of the turn.
  Scene s("ap_refill_plain", plain(12, 5), kDuel, false, [](skilltest::Copy& c) {
    c.swap("game.json", "{ \"id\": \"gold\", \"name\": \"Gold\" }", "{ \"id\": \"gold\", \"name\": \"Gold\", \"refill\": { \"perTurn\": 7, \"carryOver\": 3 } }");
    aptest::dropActionPoints(c, true);
  });
  CHECK(!s.b().apGame());
  CHECK_EQ(s.gold(0), 7);  // refilled at the start: 0 left + 7
  CHECK(s.go("lan", 3, 2));
  CHECK_STR(s.refusedMove("lan", 4, 2), "that unit has already moved");  // one move per turn, as before
  CHECK_EQ(s.b().apPool(0), 0);
  CHECK(s.endTurn());
  CHECK(s.endTurn());
  CHECK_EQ(s.gold(0), 3 + 7);  // 7 left over: 3 carry over, +7
}

// ---- moving ----

TEST(a_unit_may_move_several_times_up_to_its_move_stat_and_each_walk_is_paid) {
  Scene s("ap_split", plain(12, 5), kDuel);
  CHECK(s.go("lan", 3, 2));
  CHECK_EQ(s.ap(0), 18);
  CHECK_EQ(s.unit("lan").moveSpent, 2);
  CHECK(!s.unit("lan").done);
  CHECK(s.go("lan", 3, 4));  // two more points: the Move stat (4) is used up
  CHECK_EQ(s.ap(0), 16);
  CHECK_EQ(s.unit("lan").moveSpent, 4);
  CHECK_STR(s.refusedMove("lan", 4, 4), "that unit has no movement left");
  std::string why;
  CHECK(!s.go("lan", 4, 4, &why));
  CHECK_STR(why, "that unit has no movement left");
  CHECK(!s.b().apMayMove(s.unit("lan")));
  CHECK(!s.unit("lan").done);  // (it may still attack: unitSpent says no, as it does for a unit that has not acted)
  CHECK(!s.b().unitSpent(s.unit("lan")));
  // Next turn the walk starts over.
  CHECK(s.endTurn());
  CHECK(s.endTurn());
  CHECK_EQ(s.unit("lan").moveSpent, 0);
  CHECK(s.go("lan", 6, 4));
}

TEST(splitting_the_walk_can_be_switched_off) {
  Scene s("ap_nosplit", plain(12, 5), kDuel, false, [](skilltest::Copy& c) { c.swap("game.json", "\"splitMoves\": true", "\"splitMoves\": false"); });
  CHECK(s.go("lan", 3, 2));
  CHECK_STR(s.refusedMove("lan", 4, 2), "that unit has already moved");
  CHECK_EQ(s.b().apMovePointsLeft(s.unit("lan")), 0);
  CHECK(s.b().unitSpent(s.unit("lan")) == false);  // it may still attack (a foe in reach)
}

TEST(a_walk_costs_a_point_for_each_movement_point_of_the_squares_it_crosses) {
  Scene s("ap_woods", {"............", "..w.........", "............", "............", "............"},
          {{"lancer", "blue", 1, 1, "lan"}, {"dummy", "red", 10, 4, "foe"}});
  CHECK(s.go("lan", 2, 1));  // the woods square: 2 movement points
  CHECK_EQ(s.ap(0), 18);
  CHECK_EQ(s.unit("lan").moveSpent, 2);
}

TEST(points_beyond_the_soft_cap_cost_the_penalty_more_and_a_unit_type_may_set_its_own_price) {
  Scene s("ap_cap", plain(12, 5), {{"lancer", "blue", 1, 2, "lan"}, {"scout", "blue", 1, 4, "sc"}, {"dummy", "red", 10, 0, "foe"}},
          false, [](skilltest::Copy& c) { c.swap("game.json", "\"softCap\": 4", "\"softCap\": 2"); });
  // Lancer: 4 points, the first 2 at 1 AP, the next 2 at 2 AP.
  CHECK_EQ(s.b().apMoveCost(s.unit("lan"), 4), 6);
  CHECK(s.go("lan", 5, 2));
  CHECK_EQ(s.ap(0), 14);
  // Scout: its own type says 2 AP a point and a cap of 2: 3 points = 2 + 2 + 4.
  CHECK_EQ(s.b().apMoveCost(s.unit("sc"), 3), 8);
  CHECK(s.go("sc", 4, 4));
  CHECK_EQ(s.ap(0), 6);
  CHECK_EQ(s.b().apMoveCost(s.unit("sc"), 1), 4);  // the next point is beyond the cap already (the cap counts the turn's whole walk)
}

TEST(the_penalty_percent_rounds_up_to_a_whole_ap_for_each_point) {
  Scene s("ap_round", plain(12, 5), kDuel, false, [](skilltest::Copy& c) {
    c.swap("game.json", "\"softCap\": 4, \"softCapPenaltyPercent\": 100", "\"softCap\": 2, \"softCapPenaltyPercent\": 50");
  });
  CHECK_EQ(s.b().apMoveCost(s.unit("lan"), 4), 2 + 2 * 2);  // 1.5 AP a point becomes 2
}

TEST(a_walk_the_pool_cannot_pay_is_cut_short_in_the_reach_and_refused_with_a_reason) {
  Scene s("ap_short", plain(12, 5), kDuel, false, [](skilltest::Copy& c) { c.swap("game.json", "\"perTurn\": 20", "\"perTurn\": 3"); });
  CHECK_EQ(s.ap(0), 3);
  Reach reach;
  s.b().reachable(s.id("lan"), reach);
  CHECK(reach.canStop(s.at(4, 2)));    // 3 points
  CHECK(!reach.canStop(s.at(5, 2)));   // 4 points would cost 4 AP
  CHECK_STR(s.refusedMove("lan", 5, 2), "not enough AP");
  CHECK_STR(s.refusedMove("lan", 9, 2), "the unit cannot reach that cell");  // too far for its Move stat anyway
  CHECK(s.go("lan", 4, 2));
  CHECK_EQ(s.ap(0), 0);
  CHECK(s.b().unitSpent(s.unit("lan")));
}

// ---- attacking ----

TEST(an_attack_costs_ap_and_a_unit_attacks_once_a_turn_without_ending_its_movement) {
  Scene s("ap_attack", plain(12, 5), kContact);
  CHECK(s.hit("lan", "foe", "sword"));
  CHECK_EQ(s.ap(0), 18);
  CHECK_STR(changes(s.w->events, 0, s.apIndex()), "-2");
  CHECK_EQ(s.unit("lan").apAttacks, 1);
  CHECK_EQ(s.unit("foe").hp, 32);
  CHECK(!s.unit("lan").done);  // it may still walk
  CHECK_STR(s.refused(Action::attack(s.id("lan"), s.id("foe"), s.weapon("sword"))), "that unit has no attacks left this turn");
  CHECK(s.go("lan", 1, 4));    // the walk after the attack: 2 points, 2 AP
  CHECK_EQ(s.ap(0), 16);
  CHECK(!s.unit("lan").done);
  CHECK(s.go("lan", 0, 4));    // and 1 more
  CHECK(!s.unit("lan").done);
  CHECK(s.b().apMovePointsLeft(s.unit("lan")) == 1);
  CHECK(s.go("lan", 0, 3));    // the last one
  CHECK(s.b().unitSpent(s.unit("lan")));
}

TEST(an_attack_after_the_whole_walk_ends_the_unit) {
  Scene s("ap_attack_last", plain(12, 5), {{"lancer", "blue", 1, 2, "lan"}, {"dummy", "red", 6, 2, "foe"}});
  CHECK(s.go("lan", 5, 2));  // 4 points: all its movement
  CHECK(!s.unit("lan").done);
  CHECK(s.hit("lan", "foe", "sword"));
  CHECK(s.unit("lan").done);
  CHECK_EQ(s.ap(0), 14);
}

TEST(a_weapon_may_set_its_own_cost_and_an_area_weapon_adds_for_its_area) {
  Scene s("ap_weapons", plain(12, 5), {{"poker", "blue", 1, 2, "pok"}, {"mortarman", "blue", 1, 4, "mor"}, {"dummy", "red", 3, 2, "foe"}, {"dummy", "red", 5, 4, "far"}});
  CHECK(s.hit("pok", "foe", "spear"));
  CHECK_EQ(s.ap(0), 19);  // the spear: 1
  Action shot = Action::attack(s.id("mor"), s.id("far"), s.weapon("mortar"));
  shot.cell = s.at(5, 4);
  CHECK(s.w->apply(shot));
  CHECK_EQ(s.ap(0), 15);  // the mortar: 3 + 1 for the area
  CHECK_EQ(s.b().apAttackCost(s.id("mor"), s.weapon("mortar")), 4);
  CHECK_EQ(s.b().apAttackCost(s.id("pok"), s.weapon("sword")), 2);  // the game's cost
}

TEST(an_attack_the_pool_cannot_pay_is_refused_and_a_cheaper_weapon_still_goes) {
  Scene s("ap_attack_short", plain(12, 5), {{"lancer", "blue", 1, 2, "lan"}, {"poker", "blue", 1, 3, "pok"}, {"dummy", "red", 2, 2, "foe"}, {"dummy", "red", 3, 3, "foe2"}},
          false, [](skilltest::Copy& c) { c.swap("game.json", "\"perTurn\": 20", "\"perTurn\": 1"); });
  CHECK_STR(s.refused(Action::attack(s.id("lan"), s.id("foe"), s.weapon("sword"))), "not enough AP");
  std::string why;
  CHECK(!s.hit("lan", "foe", "sword", &why));
  CHECK_STR(why, "not enough AP");
  CHECK_EQ(s.ap(0), 1);
  CHECK(s.hit("pok", "foe2", "spear"));
  CHECK_EQ(s.ap(0), 0);
  CHECK(s.b().unitSpent(s.unit("pok")));  // attacked, and an empty pool pays no step
}

TEST(a_unit_that_cannot_pay_a_step_or_an_attack_is_spent_and_the_others_are_not) {
  Scene s("ap_spent", plain(12, 5), {{"lancer", "blue", 1, 2, "lan"}, {"archer", "blue", 1, 3, "arc"}, {"dummy", "red", 2, 2, "foe"}},
          false, [](skilltest::Copy& c) { c.swap("game.json", "\"perTurn\": 20", "\"perTurn\": 2"); });
  CHECK(!s.b().unitSpent(s.unit("lan")));
  CHECK(s.hit("lan", "foe", "sword"));
  CHECK_EQ(s.ap(0), 0);
  CHECK(s.b().unitSpent(s.unit("lan")));
  CHECK(s.b().unitSpent(s.unit("arc")));  // an empty pool: nobody can do anything
  CHECK(!s.unit("arc").done);             // (the flag itself is only for what the unit did)
  CHECK(!s.b().validate(Action::move(s.id("arc"), s.at(2, 3))));
}

TEST(artillery_that_moved_cannot_fire_and_the_walk_after_a_shot_is_allowed) {
  Scene s("ap_artillery", plain(12, 5), {{"mortarman", "blue", 1, 2, "mor"}, {"dummy", "red", 5, 2, "foe"}});
  CHECK(s.go("mor", 2, 2));
  CHECK(!s.b().apMayAttack(s.unit("mor")));  // "afterMove": false still means what it says
  Scene t("ap_artillery2", plain(12, 5), {{"mortarman", "blue", 1, 2, "mor"}, {"dummy", "red", 5, 2, "foe"}});
  Action shot = Action::attack(t.id("mor"), t.id("foe"), t.weapon("mortar"));
  shot.cell = t.at(5, 2);
  CHECK(t.w->apply(shot));
  CHECK(t.go("mor", 3, 2));  // moving after the shot is fine
}

// ---- the other actions and the lists of costs ----

TEST(a_build_cost_may_list_the_ap_resource_and_is_refused_when_it_is_short) {
  Scene s("ap_build", aptest::factoryMap());
  const int tank = s.db().findUnitType("tank");
  CHECK(s.b().canAfford(0, tank));
  CHECK(s.w->apply(Action::build(s.at(0, 4), tank)));
  CHECK_EQ(s.ap(0), 17);
  CHECK_EQ(s.gold(0), 21);  // 20 + the factory's income 6 at the start of the turn, minus the 5 paid
  // With less than 3 AP left the tank is refused with the usual reason.
  Scene t("ap_build2", aptest::factoryMap(), [](skilltest::Copy& c) { c.swap("game.json", "\"perTurn\": 20", "\"perTurn\": 2"); });
  std::string why;
  CHECK(!t.b().canAfford(0, tank));
  CHECK(!t.w->apply(Action::build(t.at(0, 4), tank), &why));
  CHECK_STR(why, "not enough resources");
}

TEST(a_technology_may_cost_ap_too) {
  Scene s("ap_research", aptest::factoryMap());
  const int drill = s.db().findTechnology("drill");
  CHECK(s.w->apply(Action::research(drill)));
  CHECK_EQ(s.ap(0), 16);
  CHECK_EQ(s.gold(0), 21);  // 20 + the factory's income 6 at the start of the turn, minus the 5 paid
}

TEST(waiting_and_other_actions_cost_nothing_and_end_the_units_turn_as_before) {
  Scene s("ap_wait", plain(12, 5), kDuel);
  CHECK(s.w->apply(Action::wait(s.id("lan"))));
  CHECK_EQ(s.ap(0), 20);
  CHECK(s.unit("lan").done);
  CHECK(s.b().unitSpent(s.unit("lan")));
  CHECK_STR(s.refusedMove("lan", 3, 2), "that unit has already acted");
}

// ---- the enemy-range display ----

TEST(the_enemy_range_ignores_the_foes_pool) {
  Scene s("ap_ranges", plain(12, 5), {{"lancer", "blue", 1, 2, "lan"}, {"dummy", "red", 8, 2, "foe"}});
  CHECK_EQ(s.ap(1), 0);  // red has nothing in its pool during blue's turn
  ThreatArea area;
  Reach scratch;
  threatArea(s.b(), s.id("foe"), 0, scratch, area);
  CHECK(area.move[(size_t)s.at(5, 2)]);   // 3 squares: its Move stat
  CHECK(area.move[(size_t)s.at(8, 2)]);
  CHECK(!area.move[(size_t)s.at(4, 2)]);  // 4 squares: more than the stat
}

// ---- saves and determinism ----

TEST(a_save_in_the_middle_of_a_turn_restores_the_counters) {
  Scene s("ap_save", plain(12, 5), kContact);
  CHECK(s.hit("lan", "foe", "sword"));
  CHECK(s.go("lan", 1, 3));
  Json j;
  saveBattleState(s.b(), j);
  const std::string text = j.dump(0);
  CHECK(text.find("\"apAttacks\":1") != std::string::npos);
  CHECK(text.find("\"moveSpent\":1") != std::string::npos);
  Scene t("ap_save2", plain(12, 5), kContact);
  std::string err;
  CHECK(restoreBattleState(t.b(), j, err));
  CHECK_STR(savetest::stateText(t.b()), savetest::stateText(s.b()));
  CHECK_EQ(t.unit("lan").apAttacks, 1);
  CHECK_EQ(t.unit("lan").moveSpent, 1);
  CHECK_EQ(t.ap(0), 17);
  CHECK_STR(t.refused(Action::attack(t.id("lan"), t.id("foe"), t.weapon("sword"))), "that unit has no attacks left this turn");
  CHECK(t.go("lan", 1, 4));  // it goes on where the original did
  CHECK(s.go("lan", 1, 4));
  CHECK_STR(savetest::stateText(t.b()), savetest::stateText(s.b()));
}

TEST(a_save_with_more_attacks_than_the_game_allows_is_refused) {
  Scene s("ap_save_bad", plain(12, 5), kContact);
  CHECK(s.hit("lan", "foe", "sword"));
  Json j;
  saveBattleState(s.b(), j);
  std::string text = j.dump(0);
  const size_t at = text.find("\"apAttacks\":1");
  CHECK(at != std::string::npos);
  text.replace(at, 13, "\"apAttacks\":5");
  Json bad;
  std::string perr;
  CHECK(Json::parse(text, bad, perr));
  Scene t("ap_save_bad2", plain(12, 5), kContact);
  std::string err;
  CHECK(!restoreBattleState(t.b(), bad, err));
  CHECK(err.find("apAttacks") != std::string::npos);
}

TEST(the_same_orders_give_the_same_battle) {
  auto play = [](const char* name) {
    Scene s(name, plain(12, 5), kContact);
    s.hit("lan", "foe", "sword");
    s.go("lan", 1, 4);
    s.endTurn();
    s.endTurn();
    s.go("lan", 2, 3);
    return std::make_pair(savetest::stateText(s.b()), s.w->events.size());
  };
  CHECK(play("ap_det1") == play("ap_det2"));
}

// ---- the loader ----

namespace {
std::string loadError(const char* name, const char* from, const char* to, const char* file = "game.json") {
  skilltest::Copy c(name, "action-points");
  c.swap(file, from, to);
  return c.gameError();
}
}  // namespace

TEST(the_loader_refuses_a_wrong_action_point_block_naming_the_file_and_the_key) {
  using skilltest::saysIn;
  CHECK(saysIn(loadError("apl1", "\"resource\": \"ap\"", "\"resource\": \"mana\""), "game.json: rules.actionPoints: \"resource\" names unknown resource \"mana\""));
  CHECK(saysIn(loadError("apl2", "\"costPerStep\": 1", "\"costPerStep\": -1"), "rules.actionPoints, \"move\": \"costPerStep\" must be a whole number from 0 to 99"));
  CHECK(saysIn(loadError("apl3", "\"attacksPerTurn\": 1", "\"attacksPerTurn\": 0"), "\"attacksPerTurn\" must be a whole number from 1 to 99"));
  CHECK(saysIn(loadError("apl4", "\"softCapPenaltyPercent\": 100", "\"softCapPenaltyPercent\": 5000"), "\"softCapPenaltyPercent\" must be a whole number from 0 to 1000"));
  CHECK(saysIn(loadError("apl5", "\"cost\": 2", "\"cost\": -3"), "rules.actionPoints, \"attack\": \"cost\" must be a whole number from 0 to 99"));
  CHECK(saysIn(loadError("apl6", "\"splitMoves\": true", "\"splitMoves\": true, \"speed\": 2"), "unknown key \"speed\""));
  CHECK(saysIn(loadError("apl7", "\"perTurn\": 20", "\"perTurn\": -1"), "resource \"ap\", \"refill\": \"perTurn\" must be a whole number from 0 to 100000"));
  CHECK(saysIn(loadError("apl8", "\"carryOver\": 10", "\"carryOver\": 10, \"x\": 1"), "resource \"ap\", \"refill\": unknown key \"x\""));
  CHECK(saysIn(loadError("apl9", "\"refill\": { \"perTurn\": 20, \"carryOver\": 10 }", "\"refill\": { \"carryOver\": 10 }"), "needs \"perTurn\""));
}

TEST(the_loader_refuses_ap_keys_of_weapons_and_unit_types_in_a_game_without_action_points) {
  using skilltest::saysIn;
  skilltest::Copy c("apl_w", "action-points");
  aptest::dropActionPoints(c);
  CHECK(saysIn(c.gameError(), "weapons.json: weapon \"spear\": \"apCost\" needs \"rules\": { \"actionPoints\""));
  c.swap("data/weapons.json", ", \"apCost\": 1", "");
  c.swap("data/weapons.json", ", \"apCost\": 3", "");
  CHECK(saysIn(c.gameError(), "\"area\".\"apExtra\" needs"));
  c.swap("data/weapons.json", ", \"apExtra\": 1", "");
  CHECK(saysIn(c.gameError(), "units.json: unit \"scout\": \"ap\" needs"));
  c.swap("data/units.json", ", \"ap\": { \"step\": 2, \"softCap\": 2 }", "");
  CHECK_STR(c.gameError(), "");
  // With the rules on, the weapon and unit keys are checked too.
  skilltest::Copy d("apl_w2", "action-points");
  d.swap("data/weapons.json", "\"apCost\": 1", "\"apCost\": -1");
  CHECK(saysIn(d.gameError(), "weapon \"spear\": \"apCost\" must be a whole number from 0 to 99"));
  skilltest::Copy e("apl_w3", "action-points");
  e.swap("data/units.json", "\"step\": 2", "\"step\": 2, \"cost\": 1");
  CHECK(saysIn(e.gameError(), "unit \"scout\", \"ap\": unknown key \"cost\""));
}
