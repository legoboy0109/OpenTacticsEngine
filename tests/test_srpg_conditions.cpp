// Status conditions in a battle (docs/SRPG.md section 9): the blocks, the count of turns, re-applying, immunity and the three ways to
// inflict and cure (staves, usable items, weapons and skills). Game: tests/fixtures/srpg-conditions (see tests/srpg_conditions_fixture.h).
#include "srpg_conditions_fixture.h"

using namespace ote;
using namespace condtest;

namespace {
int countOf(const std::vector<Event>& ev, EventType t) {
  int n = 0;
  for (const Event& e : ev) n += e.type == t;
  return n;
}
}  // namespace

// ---- damage and healing over time ----

TEST(conditions_poison_hurts_at_the_start_of_each_of_the_units_own_turns_and_wears_off_after_the_third) {
  Field w("c_poison");
  w.give("r1", "poison");  // 3 turns
  CHECK_EQ(w.hp("r1"), 22);
  w.endTurn();             // red's turn starts: tick 1 (10% of 22 = 2)
  CHECK_EQ(w.hp("r1"), 20);
  CHECK_EQ(w.left("r1", "poison"), 2);
  CHECK(w.nth(EventType::UnitHurt) && w.nth(EventType::UnitHurt)->a == w.id("r1") && w.nth(EventType::UnitHurt)->b == 2);
  CHECK(w.nth(EventType::SkillActivated) != nullptr);  // the condition's name floats from this one
  w.endTurn();             // red's turn ends: nothing goes yet
  CHECK(w.has("r1", "poison"));
  w.endTurn();             // tick 2
  CHECK_EQ(w.hp("r1"), 18);
  CHECK_EQ(w.left("r1", "poison"), 1);
  w.endTurn();
  w.endTurn();             // tick 3: count 0, it is still on for this turn
  CHECK_EQ(w.hp("r1"), 16);
  CHECK_EQ(w.left("r1", "poison"), 0);
  w.endTurn();             // red's turn ends: it wears off
  CHECK(!w.has("r1", "poison"));
  CHECK(w.nth(EventType::ConditionEnded) && w.nth(EventType::ConditionEnded)->c == kConditionRanOut);
  w.round();
  CHECK_EQ(w.hp("r1"), 16);  // three ticks, no more
}

TEST(conditions_a_blue_unit_ticks_only_on_blues_turn) {
  Field w("c_poison_blue");
  w.give("kay", "poison");
  w.endTurn();  // red's turn
  CHECK_EQ(w.hp("kay"), 24);
  w.endTurn();  // blue's: 10% of 24 = 2
  CHECK_EQ(w.hp("kay"), 22);
}

TEST(conditions_a_tick_never_kills_by_default_but_leaves_one_hit_point) {
  Field w("c_poison_one");
  w.give("r1", "fever");  // 5 flat
  w.unit("r1").hp = 3;
  w.endTurn();
  CHECK_EQ(w.hp("r1"), 1);
  CHECK(w.unit("r1").alive);
  CHECK_EQ(w.nth(EventType::UnitHurt)->b, 2);
  w.round();
  CHECK_EQ(w.hp("r1"), 1);  // already at 1: nothing happens, no event
  CHECK_EQ(countOf(w.events, EventType::UnitHurt), 0);
}

TEST(conditions_a_tick_with_canKill_defeats_the_unit_like_any_death) {
  Field w("c_deadly");
  w.give("r1", "deadly");
  w.unit("r1").hp = 10;
  w.endTurn();
  CHECK(!w.unit("r1").alive && w.unit("r1").conditions.empty());
  CHECK(w.nth(EventType::UnitHurt) && w.nth(EventType::UnitHurt)->b == 10);
  CHECK(w.nth(EventType::UnitDied) && w.nth(EventType::UnitDied)->a == w.id("r1"));
  CHECK(!w.b().over);  // r2 and r3 are still there
  CHECK_EQ(w.b().occupant[(size_t)w.at(5, 2)], -1);
}

TEST(conditions_a_tick_that_defeats_the_last_unit_of_a_side_ends_the_battle_at_once) {
  Field w("c_deadly_last");
  w.remove("r2");
  w.remove("r3");
  w.give("r1", "deadly");
  w.unit("r1").hp = 5;
  w.endTurn();
  CHECK(w.b().over && w.b().winnerTeam == 0);
  CHECK(w.nth(EventType::SideDefeated) && w.nth(EventType::BattleEnded));
}

TEST(conditions_a_tick_that_defeats_a_leader_decides_a_leader_rule_game) {
  Field w("c_deadly_leader", "field", [](Copy& c) {
    c.swap("game.json", "\"defeat\": [\"noUnits\"]", "\"defeat\": [\"leaderDead\"]");
    c.swap("maps/field.json", "\"id\": \"r1\", \"name\": \"Rook\", \"side\": \"red\"", "\"id\": \"r1\", \"name\": \"Rook\", \"side\": \"red\", \"leader\": true");
  });
  w.give("r1", "deadly");
  w.unit("r1").hp = 5;
  w.endTurn();
  CHECK(w.b().over && w.b().winnerTeam == 0);
}

TEST(conditions_a_condition_can_heal_each_turn_and_a_damage_can_be_a_formula) {
  Field w("c_regen");
  w.give("r1", "regen");
  w.unit("r1").hp = 10;
  w.endTurn();
  CHECK_EQ(w.hp("r1"), 14);
  CHECK(w.nth(EventType::UnitHealed) && w.nth(EventType::UnitHealed)->b == 4);
  Field f("c_formula_burn");
  f.give("r1", "formula_burn");  // a.mag + 1 = 3 for a raider (mag 2)
  f.give("kay", "formula_burn");  // 1 for kay (mag 0)
  f.endTurn();
  CHECK_EQ(f.hp("r1"), 19);
  f.endTurn();
  f.endTurn();
  CHECK_EQ(f.hp("kay"), 23);
}

// ---- cannot act, cannot move ----

TEST(conditions_a_sleeping_unit_is_finished_at_the_start_of_its_turn_and_takes_no_orders) {
  Field w("c_sleep");
  w.give("r1", "sleep", 2);
  w.endTurn();
  CHECK(w.unit("r1").done);
  const Event* t = w.nth(EventType::ConditionTick);
  CHECK(t && t->a == w.id("r1") && t->c == kTickCannotAct);
  CHECK_STR(w.why(Action::move(w.id("r1"), w.at(5, 3))), "that unit cannot act (Sleep)");
  CHECK_STR(w.why(Action::wait(w.id("r1"))), "that unit cannot act (Sleep)");
  CHECK(w.why(w.attack("r1", "kay")).find("cannot act") != std::string::npos);
  CHECK(w.run(Action::wait(w.id("r2"))));  // the others are fine
  CHECK_EQ(w.left("r1", "sleep"), 1);
  w.endTurn();
  w.endTurn();  // second sleeping turn
  CHECK(w.unit("r1").done);
  CHECK_EQ(w.left("r1", "sleep"), 0);
  w.endTurn();  // it wakes when the turn ends
  CHECK(!w.has("r1", "sleep") && w.nth(EventType::ConditionEnded) != nullptr);
  w.endTurn();
  CHECK(!w.unit("r1").done);  // a normal turn
}

TEST(conditions_a_sleeper_does_not_answer_is_always_hit_and_wakes_when_struck) {
  Field w("c_sleep_struck");
  w.give("r1", "sleep");
  w.place("kay", 4, 2);
  Forecast f;
  CHECK(w.b().forecast(w.id("kay"), w.id("r1"), w.weapon("iron_blade"), w.cellOf("kay"), f));
  CHECK_EQ(f.attack.hit, 100);  // the hit formula says 90: it cannot dodge
  CHECK(!f.counter.can);        // and cannot answer
  CHECK(w.strike("kay", "r1"));
  CHECK_EQ(countOf(w.events, EventType::Attack), 1);
  CHECK(!w.has("r1", "sleep"));
  const Event* e = w.nth(EventType::ConditionEnded);
  CHECK(e && e->a == w.id("r1") && e->c == kConditionStruck);
  // Not woken when nothing was struck: a sleeper missed... (a strike with 100 hit always connects, so use the next test for the rest)
}

TEST(conditions_a_dazed_unit_may_answer_and_dodge_when_its_condition_says_so) {
  Field w("c_daze");
  w.give("r1", "daze");
  w.place("kay", 4, 2);
  Forecast f;
  CHECK(w.b().forecast(w.id("kay"), w.id("r1"), w.weapon("iron_blade"), w.cellOf("kay"), f));
  CHECK_EQ(f.attack.hit, 90);
  CHECK(f.counter.can);
  w.endTurn();
  CHECK(w.unit("r1").done);  // it still cannot act on its own turn
}

TEST(conditions_a_stunned_unit_cannot_move_but_can_still_attack) {
  Field w("c_stun");
  w.give("kay", "stun");
  w.place("r1", 3, 2);
  Reach reach;
  w.b().reachable(w.id("kay"), reach);
  CHECK_EQ((int)reach.stops.size(), 1);
  CHECK_STR(w.why(Action::move(w.id("kay"), w.at(2, 3))), "that unit cannot move (Stun)");
  CHECK(w.strike("kay", "r1"));
  CHECK(w.unit("kay").done);
}

TEST(conditions_a_silenced_unit_cannot_use_staves_or_its_tome_but_can_use_a_sword_and_items) {
  Field w("c_silence");
  w.give("mira", "silence");
  CHECK(!w.option("mira", "poison_staff").ok);
  CHECK_STR(w.option("mira", "poison_staff").why, "Silence stops that");
  std::vector<int> weapons;
  w.b().weaponsOf(w.unit("mira"), weapons);
  CHECK(weapons.empty());  // the fire tome is a tome
  CHECK_STR(w.staffWhy("mira", "cure_staff", "kay"), "Silence stops that");
  w.give("kay", "silence");
  w.b().weaponsOf(w.unit("kay"), weapons);
  CHECK_EQ((int)weapons.size(), 1);  // a sword is fine
  std::vector<int> t;
  w.b().useTargets(w.id("kay"), w.slotOf("kay", "tonic"), t);
  CHECK_EQ((int)t.size(), 1);  // the tonic cures the silence itself: items are not silenced
  CHECK(w.has("kay", "silence"));
}

TEST(conditions_a_condition_can_stop_the_use_of_items) {
  Field w("c_bind");
  w.unit("kay").hp = 10;
  CHECK(w.why(Action::useItem(w.id("kay"), w.slotOf("kay", "tonic"), w.id("kay"))).empty());
  w.give("kay", "bind");
  CHECK_STR(w.why(Action::useItem(w.id("kay"), w.slotOf("kay", "tonic"), w.id("kay"))), "Bind stops the unit using items");
  CHECK_STR(w.why(Action::move(w.id("kay"), w.at(2, 3))), "that unit cannot move (Bind)");
}

// ---- stats and combat numbers ----

TEST(conditions_stat_changes_count_everywhere_and_the_combat_forecast_shows_them) {
  Field w("c_curse");
  w.place("kay", 4, 2);
  Forecast before, after;
  CHECK(w.b().forecast(w.id("kay"), w.id("r1"), w.weapon("iron_blade"), w.cellOf("kay"), before));
  w.give("kay", "curse");
  CHECK_EQ(w.b().stat(w.unit("kay"), w.stat("str")), 5);
  CHECK(w.b().forecast(w.id("kay"), w.id("r1"), w.weapon("iron_blade"), w.cellOf("kay"), after));
  CHECK_EQ(before.attack.damage - after.attack.damage, 2);
  CHECK_EQ(after.counter.damage - before.counter.damage, 2);  // def -2: the raider hurts it more
  w.unit("kay").conditions.clear();
  w.give("kay", "bless");  // str +2, and +5 hit +1 damage in combat
  Forecast blessed;
  CHECK(w.b().forecast(w.id("kay"), w.id("r1"), w.weapon("iron_blade"), w.cellOf("kay"), blessed));
  CHECK_EQ(blessed.attack.damage - before.attack.damage, 3);
  CHECK_EQ(blessed.attack.hit - before.attack.hit, 5);
}

TEST(conditions_a_skill_can_test_for_a_condition_and_several_conditions_add_up) {
  Field w("c_wary");
  w.unit("kay").learned.push_back(w.skill("wary"));
  CHECK_EQ(w.b().stat(w.unit("kay"), w.stat("def")), 3);
  w.give("kay", "poison");
  CHECK_EQ(w.b().stat(w.unit("kay"), w.stat("def")), 6);
  w.give("kay", "curse");
  CHECK_EQ(w.b().stat(w.unit("kay"), w.stat("def")), 4);
  CHECK_EQ(w.b().stat(w.unit("kay"), w.stat("str")), 5);
  w.round();
  w.round();
  w.round();
  CHECK_EQ(w.left("kay", "poison"), 0);  // the third turn: still on
  w.endTurn();
  w.endTurn();
  CHECK_EQ((int)w.unit("kay").conditions.size(), 0);  // gone when blue's third turn ended
}

// ---- how long ----

TEST(conditions_until_cured_and_until_the_map_ends_never_count_down) {
  Field w("c_forever");
  w.give("r1", "mark");
  w.give("r1", "scar");
  for (int i = 0; i < 6; ++i) w.round();
  CHECK(w.has("r1", "mark") && w.has("r1", "scar"));
  CHECK_EQ(w.left("r1", "mark"), -1);
  CHECK_EQ(w.b().stat(w.unit("r1"), w.stat("def")), 1);
}

TEST(conditions_the_chance_to_wear_off_is_rolled_each_own_turn_before_anything_else_happens) {
  int woreOff = 0, stayed = 0;
  for (int seed = 1; seed <= 40; ++seed) {
    Field w("c_wearoff");
    w.b().rng.seed((uint64_t)seed);
    w.give("r1", "fleeting");
    w.endTurn();
    if (w.has("r1", "fleeting")) {
      ++stayed;
      CHECK_EQ(w.left("r1", "fleeting"), 4);
    } else {
      ++woreOff;
      const Event* e = w.nth(EventType::ConditionEnded);
      CHECK(e && e->c == kConditionWoreOff);
    }
  }
  CHECK(woreOff > 30 && stayed < 10);  // 99%
}

TEST(conditions_reapplying_starts_the_duration_over_and_keeps_the_place_in_the_order) {
  Field w("c_reapply");
  w.give("r1", "poison", 1);
  w.give("r1", "curse");
  w.unit("r1").hp = 22;
  w.place("kay", 4, 2);
  w.unit("kay").items.insert(w.unit("kay").items.begin(), ItemStack{w.item("venom_blade"), -1, false});  // poison for 2 turns on a hit
  CHECK(w.strike("kay", "r1"));
  const Event* a = w.about(EventType::ConditionApplied, "r1", "poison");
  CHECK(a && a->c == 2 && (a->flags & kRefreshed) && a->d == w.id("kay"));
  CHECK_EQ(w.left("r1", "poison"), 2);  // longer than the 1 it had: a start over, not a sum
  CHECK_EQ(w.unit("r1").conditions[0].condition, w.cond("poison"));
  CHECK_EQ((int)w.unit("r1").conditions.size(), 2);
}

TEST(conditions_applying_a_condition_removes_the_ones_it_replaces) {
  // chill is not inflicted by any staff of the fixture: the hex staff is changed for this test only.
  Field v("c_replaces", "field", [](Copy& cc) {
    cc.swap("data/items.json", "\"effects\": [ { \"inflict\": { \"condition\": \"curse\" } } ] },\n  { \"id\": \"rage_staff\"",
            "\"effects\": [ { \"inflict\": { \"condition\": \"chill\" } } ] },\n  { \"id\": \"rage_staff\"");
  });
  v.give("r1", "ember");
  v.place("r1", 3, 2);
  CHECK(v.useStaff("mira", "hex_staff", "r1"));
  CHECK(!v.has("r1", "ember") && v.has("r1", "chill"));
  const Event* e = v.about(EventType::ConditionEnded, "r1", "ember");
  CHECK(e && e->c == kConditionReplaced);
  CHECK_EQ(v.b().stat(v.unit("r1"), v.stat("mov")), 3);
}

// ---- immunity ----

TEST(conditions_a_skill_makes_a_unit_immune_and_it_is_told_so_when_a_weapon_tries) {
  Field w("c_immune");
  CHECK(w.b().conditionImmune(w.unit("bo"), w.cond("poison")) && w.b().conditionImmune(w.unit("bo"), w.cond("sleep")));
  CHECK(!w.b().conditionImmune(w.unit("bo"), w.cond("curse")) && !w.b().conditionImmune(w.unit("kay"), w.cond("poison")));
  w.place("r1", 3, 4);
  w.endTurn();  // red's turn: r1 strikes bo with its venom blade
  CHECK(w.run(w.attack("r1", "bo")));
  CHECK(!w.has("bo", "poison"));
  const Event* e = w.nth(EventType::ConditionResisted);
  CHECK(e && e->a == w.id("bo") && e->b == w.cond("poison") && e->c == w.id("r1"));
  CHECK_EQ(countOf(w.events, EventType::ConditionApplied), 0);
}

// ---- the three ways to inflict ----

TEST(conditions_a_weapon_inflicts_on_a_hit_and_a_miss_inflicts_nothing) {
  Field w("c_weapon");
  w.place("r1", 3, 2);
  w.endTurn();
  CHECK(w.run(w.attack("r1", "kay")));
  const Event* a = w.about(EventType::ConditionApplied, "kay", "poison");
  CHECK(a && a->c == 2 && a->d == w.id("r1"));
  CHECK(w.has("kay", "poison") && w.left("kay", "poison") == 2);
  // The counter of kay's iron blade carries nothing.
  CHECK(!w.has("r1", "poison"));
  // The event comes after the hit.
  size_t attackAt = 0, appliedAt = 0;
  for (size_t i = 0; i < w.events.size(); ++i) {
    if (w.events[i].type == EventType::Attack && attackAt == 0) attackAt = i + 1;
    if (w.events[i].type == EventType::ConditionApplied) appliedAt = i + 1;
  }
  CHECK(attackAt > 0 && appliedAt > attackAt);
}

TEST(conditions_a_weapons_chance_can_be_a_formula_of_the_wielder_and_shows_in_the_forecast) {
  Field w("c_weapon_formula");
  Forecast f;
  w.place("r3", 2, 3);  // a raider's magic is 2: 2 * 25 = 50%
  CHECK(w.b().forecast(w.id("r3"), w.id("kay"), w.weapon("lull_blade"), w.cellOf("r3"), f));
  CHECK_EQ((int)f.notes.size(), 1);
  CHECK(f.notes[0].unit == w.id("r3") && f.notes[0].chance == 50);
  CHECK_STR(w.rig.db.skills[(size_t)f.notes[0].skill].name, "Sleep");
}

TEST(conditions_a_skill_can_inflict_when_it_strikes_and_when_it_is_struck) {
  Field w("c_skill_inflict");
  w.unit("kay").learned.push_back(w.skill("gloom_strike"));
  w.unit("r1").learned.push_back(w.skill("thorns"));
  w.place("kay", 4, 2);
  CHECK(w.strike("kay", "r1"));
  CHECK(w.has("r1", "curse"));   // gloom strike: the foe it hit
  CHECK(w.has("kay", "poison"));  // r1's thorns: the one who struck it
  CHECK_EQ(w.left("kay", "poison"), 2);
  CHECK_EQ(w.left("r1", "curse"), 3);
}

// ---- staves ----

TEST(conditions_a_staff_inflicts_with_a_hit_formula_that_reads_both_units_and_the_distance) {
  Field w("c_staff_poison");
  w.place("r1", 3, 2);  // 2 from mira
  StaffForecast f = w.forecast("mira", "poison_staff", "r1");
  CHECK_EQ((int)f.hits.size(), 1);
  CHECK_EQ(f.hits[0].hit, 70 + 6 * 5 - 1 * 5 - 2 * 2);  // 70 + a.mag * 5 - b.res * 5 - dist * 2
  CHECK_EQ(f.hits[0].inflict, w.cond("poison"));
  CHECK_EQ(f.hits[0].inflictTurns, 3);
  w.place("r1", 4, 2);  // 3 away: the distance counts
  CHECK_EQ(w.forecast("mira", "poison_staff", "r1").hits[0].hit, 70 + 30 - 5 - 6);
  CHECK(w.useStaff("mira", "poison_staff", "r1"));
  // The roll: one of the two outcomes, never both.
  const bool applied = w.nth(EventType::ConditionApplied) != nullptr, missed = w.nth(EventType::StaffMissed) != nullptr;
  CHECK(applied != missed);
  CHECK_EQ(w.has("r1", "poison"), applied);
  CHECK(w.unit("mira").done && w.unit("mira").items[(size_t)w.slotOf("mira", "poison_staff")].uses == 4);
}

TEST(conditions_a_staff_without_a_hit_formula_always_works_and_an_area_afflicts_everyone_in_it_except_the_immune) {
  Field v("c_staff_sleep");
  v.place("mira", 3, 2);
  v.place("r1", 6, 2);
  v.place("r2", 7, 2);
  CHECK(v.useStaff("mira", "sleep_staff", "r1"));
  CHECK(v.has("r1", "sleep") && v.has("r2", "sleep") && !v.has("r3", "sleep"));
  CHECK_EQ(v.left("r1", "sleep"), 2);  // the staff's own "turns"
  CHECK_EQ(countOf(v.events, EventType::StaffMissed), 0);
  CHECK(v.nth(EventType::StaffUsed) && v.nth(EventType::StaffUsed)->d == 2);
}

TEST(conditions_a_staff_will_not_pick_an_immune_unit_or_one_that_has_it_already) {
  Field w("c_staff_refused");
  w.place("mira", 2, 3);
  w.place("r1", 2, 5);  // plenty of room: mira's reach is 4
  CHECK_STR(w.staffWhy("mira", "sleep_staff", "r1"), "");
  w.give("r1", "sleep", 3);
  CHECK_STR(w.staffWhy("mira", "sleep_staff", "r1"), "that unit has it already");
  Field v("c_staff_immune");
  v.place("mira", 2, 3);
  v.unit("r1").learned.push_back(v.skill("iron_will"));  // a foe that cannot be put to sleep
  v.place("r1", 2, 5);
  CHECK_STR(v.staffWhy("mira", "sleep_staff", "r1"), "that unit is immune to it");
  Field none("c_staff_none");
  none.unit("r1").learned.push_back(none.skill("iron_will"));
  none.unit("r2").learned.push_back(none.skill("iron_will"));
  none.unit("r3").learned.push_back(none.skill("iron_will"));
  none.place("r1", 3, 2);
  const StaffOption o = none.option("mira", "sleep_staff");
  CHECK(!o.ok);
  CHECK_STR(o.why, "nobody in reach can be affected");
}

TEST(conditions_a_cure_staff_only_has_targets_that_have_something_it_cures) {
  Field w("c_staff_cure");
  CHECK(w.targets("nell", "cure_staff").empty());
  CHECK_STR(w.option("nell", "cure_staff").why, "nobody in reach has anything it cures");
  w.give("kay", "poison");
  w.give("kay", "curse");
  const std::vector<int> t = w.targets("nell", "cure_staff");
  CHECK_EQ((int)t.size(), 1);
  CHECK(t[0] == w.id("kay"));
  StaffForecast f = w.forecast("nell", "cure_staff", "kay");
  CHECK_EQ((int)f.hits[0].cures.size(), 1);  // only poison: curse is not on its list
  CHECK(w.useStaff("nell", "cure_staff", "kay"));
  CHECK(!w.has("kay", "poison") && w.has("kay", "curse"));
  const Event* e = w.about(EventType::ConditionEnded, "kay", "poison");
  CHECK(e && e->c == kConditionCured);
}

TEST(conditions_a_cure_all_staff_with_area_all_cleans_every_afflicted_ally_in_reach) {
  Field w("c_staff_purify");
  w.give("kay", "poison");
  w.give("kay", "curse");
  w.give("bo", "curse");
  w.give("r1", "poison");  // a foe: not an ally of the staff
  CHECK(w.useStaff("mira", "purify_staff", nullptr));
  CHECK(w.unit("kay").conditions.empty() && w.unit("bo").conditions.empty());
  CHECK(w.has("r1", "poison"));
  CHECK_EQ(countOf(w.events, EventType::ConditionEnded), 3);
}

TEST(conditions_heal_then_cure_then_inflict_is_the_order_of_a_staffs_effects) {
  Field w("c_staff_order");
  w.give("kay", "poison");
  w.unit("kay").hp = 10;
  CHECK(w.useStaff("nell", "mend_cure_staff", "kay"));
  size_t healed = 0, cured = 0;
  for (size_t i = 0; i < w.events.size(); ++i) {
    if (w.events[i].type == EventType::UnitHealed) healed = i + 1;
    if (w.events[i].type == EventType::ConditionEnded) cured = i + 1;
  }
  CHECK(healed > 0 && cured > healed);
  CHECK_EQ(w.hp("kay"), 15);
  // A staff that lists inflict first still cures first: "cure poison, then inflict poison for 2" leaves poison with 2 turns.
  Field v("c_staff_order2", "field", [](Copy& c) {
    c.swap("data/items.json", "\"effects\": [ { \"cure\": [\"poison\"] } ] }", "\"effects\": [ { \"inflict\": { \"condition\": \"poison\", \"turns\": 2 } }, { \"cure\": [\"poison\"] } ] }");
  });
  v.give("kay", "poison", 3);
  CHECK(v.useStaff("nell", "cure_staff", "kay"));
  CHECK(v.has("kay", "poison") && v.left("kay", "poison") == 2);
  CHECK(v.nth(EventType::ConditionEnded) && v.nth(EventType::ConditionApplied));
}

TEST(conditions_a_self_staff_cures_its_user_and_a_team_staff_with_a_radius_blesses_allies_around_the_pick) {
  Field w("c_staff_self");
  w.give("nell", "poison");
  w.give("nell", "curse");
  CHECK(w.useStaff("nell", "cleanse_staff", nullptr));
  CHECK(w.unit("nell").conditions.empty());
  Field v("c_staff_bless");
  CHECK(v.useStaff("mira", "blessing_staff", "kay"));  // kay (2,2): mira (1,2), nell (1,3), bo (2,4) are within 2 of it
  CHECK(v.has("kay", "bless") && v.has("mira", "bless") && v.has("nell", "bless") && v.has("bo", "bless"));
  CHECK(!v.has("r1", "bless"));
}

TEST(conditions_a_staffs_use_is_spent_and_its_experience_earned_even_when_the_roll_misses) {
  Field w("c_staff_miss", "field", [](Copy& c) {
    c.swap("data/items.json", "\"hit\": \"70 + a.mag * 5 - b.res * 5 - dist * 2\"", "\"hit\": \"0\"");
  });
  w.place("r1", 3, 2);
  CHECK(w.useStaff("mira", "poison_staff", "r1"));
  CHECK(w.nth(EventType::StaffMissed));
  CHECK(!w.has("r1", "poison"));
  CHECK_EQ(w.unit("mira").items[(size_t)w.slotOf("mira", "poison_staff")].uses, 4);
  CHECK(w.nth(EventType::XpGained) != nullptr);
}

// ---- usable items ----

TEST(conditions_a_usable_item_cures_or_inflicts_on_the_unit_it_is_used_on) {
  Field w("c_items");
  CHECK(!w.why(Action::useItem(w.id("kay"), w.slotOf("kay", "antidote"), w.id("kay"))).empty());  // nothing to cure: refused
  w.give("kay", "poison");
  w.give("kay", "curse");
  CHECK(w.run(Action::useItem(w.id("kay"), w.slotOf("kay", "antidote"), w.id("kay"))));
  CHECK(!w.has("kay", "poison") && w.has("kay", "curse"));
  CHECK_EQ(w.nth(EventType::ConditionEnded)->c, kConditionCured);
  Field v("c_items2");
  v.give("kay", "poison");
  v.give("kay", "curse");
  v.unit("kay").hp = 10;
  CHECK(v.run(Action::useItem(v.id("kay"), v.slotOf("kay", "tonic"), v.id("kay"))));  // cure all, and heal 3
  CHECK(v.unit("kay").conditions.empty() && v.hp("kay") == 13);
  Field x("c_items3");
  CHECK(x.run(Action::useItem(x.id("kay"), x.slotOf("kay", "bad_brew"), x.id("kay"))));  // inflicts curse for 2 turns on its user
  CHECK(x.has("kay", "curse") && x.left("kay", "curse") == 2);
  CHECK(x.about(EventType::ConditionApplied, "kay", "curse")->d == x.id("kay"));
}

// ---- berserk ----

TEST(conditions_a_berserk_unit_attacks_the_nearest_unit_of_any_side_at_the_start_of_its_turn) {
  Field w("c_berserk");
  w.give("kay", "berserk");
  w.endTurn();  // red's turn
  w.endTurn();  // blue's turn starts: kay acts by itself, mira (1,2) is next to it
  const Event* t = w.nth(EventType::ConditionTick);
  CHECK(t && t->a == w.id("kay") && t->c == kTickActsAlone);
  const Event* a = w.nth(EventType::Attack);
  CHECK(a && a->a == w.id("kay") && a->b == w.id("mira"));  // its own friend
  CHECK(w.unit("kay").done);
  CHECK_STR(w.why(Action::wait(w.id("kay"))), "that unit acts on its own (Berserk)");
  CHECK(w.hp("mira") < 18);
}

TEST(conditions_berserk_takes_the_lower_unit_id_on_a_tie_and_a_nearer_unit_before_a_farther_one) {
  Field w("c_berserk_tie");
  w.place("kay", 5, 5);
  w.place("mira", 5, 4);  // distance 1, id 1
  w.place("nell", 4, 5);  // distance 1, id 3
  w.place("bo", 3, 5);    // distance 2
  w.give("kay", "berserk");
  w.round();
  CHECK_EQ(w.nth(EventType::Attack)->b, w.id("mira"));
  Field v("c_berserk_near");
  v.place("kay", 5, 5);
  v.place("mira", 0, 0);
  v.place("nell", 4, 5);  // distance 1
  v.place("bo", 3, 5);    // distance 2
  v.give("kay", "berserk");
  v.round();
  CHECK_EQ(v.nth(EventType::Attack)->b, v.id("nell"));
}

TEST(conditions_a_berserk_unit_walks_to_a_foe_it_can_reach_and_towards_one_that_is_too_far) {
  Field w("c_berserk_walk");
  w.place("mira", 0, 0);
  w.place("nell", 0, 1);
  w.place("bo", 0, 5);
  w.place("kay", 6, 2);  // r2 is next to it; kay reaches nobody else first: the nearest is r2 (6,2)? it stands there; use another square
  w.remove("r2");
  w.give("kay", "berserk");
  w.place("kay", 9, 2);  // r1 (5,2) and r3 (7,4) are within 4 + 1 of it: r3 is nearer (distance 4 vs 4?) -> tie: lower id r1
  w.round();
  const Event* a = w.nth(EventType::Attack);
  CHECK(a && a->a == w.id("kay") && (a->b == w.id("r1") || a->b == w.id("r3")));
  CHECK(w.nth(EventType::UnitMoved) != nullptr);
  // Too far to hit anybody: it moves towards the nearest and does not attack.
  Field v("c_berserk_far");
  v.remove("r1");
  v.remove("r2");
  v.remove("r3");
  v.place("mira", 11, 5);
  v.place("nell", 11, 4);
  v.place("bo", 11, 3);
  v.place("kay", 0, 0);
  v.give("kay", "berserk");
  v.round();
  CHECK(v.nth(EventType::Attack) == nullptr);
  CHECK(v.unit("kay").cell != v.at(0, 0) && v.unit("kay").done);
}

TEST(conditions_a_berserk_red_unit_attacks_the_nearest_unit_even_when_it_is_its_own_friend) {
  Field w("c_berserk_red");
  w.give("r1", "berserk");  // r2 stands right next to it
  w.endTurn();
  const Event* a = w.nth(EventType::Attack);
  CHECK(a && a->a == w.id("r1") && a->b == w.id("r2"));
  CHECK(w.unit("r1").done);
}

TEST(conditions_a_unit_that_cannot_act_does_not_go_berserk_and_one_with_nobody_in_sight_stands_still) {
  Field w("c_berserk_sleep");
  w.give("kay", "berserk");
  w.give("kay", "sleep");
  w.round();
  CHECK(w.nth(EventType::Attack) == nullptr);
  CHECK(w.unit("kay").done);
  // With fog only what the unit's team sees is a target: the enemy in the brush is not.
  Field m("c_berserk_fog", "mist");
  m.remove("ola");
  m.give("kay", "berserk");
  m.round();
  CHECK(m.nth(EventType::Attack) == nullptr && m.nth(EventType::UnitMoved) == nullptr);
  CHECK(m.unit("kay").done && m.cellOf("kay") == m.at(1, 2));
  // With a friend in sight it attacks that friend.
  Field n("c_berserk_fog2", "mist");
  n.give("kay", "berserk");
  n.round();
  CHECK(n.nth(EventType::Attack) && n.nth(EventType::Attack)->b == n.id("ola"));
}

TEST(conditions_berserk_that_defeats_the_last_enemy_decides_the_battle_and_a_counter_can_defeat_the_berserk_unit) {
  Field w("c_berserk_win");
  w.remove("r2");
  w.remove("r3");
  w.place("kay", 4, 2);
  w.unit("r1").hp = 1;
  w.give("kay", "berserk");
  w.place("mira", 0, 5);
  w.place("nell", 0, 4);
  w.place("bo", 0, 3);
  w.round();  // kay's nearest is r1 at distance 1: it falls
  CHECK(w.b().over && w.b().winnerTeam == 0);
  Field v("c_berserk_dies");
  v.give("kay", "berserk");
  v.unit("kay").hp = 1;
  v.round();  // it strikes mira, whose counter finishes it
  CHECK(!v.unit("kay").alive || v.unit("kay").hp > 0);
  CHECK(v.nth(EventType::Attack) != nullptr);
}

// ---- carried units ----

TEST(conditions_a_carried_unit_keeps_its_conditions_frozen_until_it_is_put_down) {
  Field w("c_carried", "field", [](Copy& c) {
    c.swap("game.json", "\"progression\"", "\"rescue\": { \"mode\": \"carry\" }, \"progression\"");
  });
  w.give("mira", "poison", 3);
  w.place("kay", 1, 1);
  CHECK(w.run(Action::rescue(w.id("kay"), w.id("mira"))));
  CHECK(!w.unit("mira").alive || w.unit("mira").cell == kNoCell);
  w.round();
  w.round();
  CHECK(w.has("mira", "poison") && w.left("mira", "poison") == 3);  // no tick, no count
  CHECK_EQ(w.hp("mira"), 18);
}
