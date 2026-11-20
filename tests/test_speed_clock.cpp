// Docs/TURN_ORDER.md "The clock", "Rounds", "A turn's cost" and the loader's refusals: who has the turn, the tie rules, what a turn costs,
// per-side effects once a round and per-unit ones in the unit's own turn, stopped units, the "refresh" skill, hot seat, load refusals. Fixture
// tests/fixtures/speed-order (numbers in speed_fixture.h).
#include "speed_fixture.h"

using namespace ote;
using speedtest::Scene;
using speedtest::plain;
using speedtest::U;

namespace {

const std::vector<U> kField{{"knight", "blue", 1, 2, "kn"}, {"scout", "blue", 1, 3, "sc"}, {"dummy", "red", 9, 2, "foe"}, {"dummy", "red", 10, 3, "foe2"}};

// A whole map with a camp for each side (5 gold a round each), written for the test.
const char* kCampMap =
    "{ \"name\": \"T\", \"grid\": { \"type\": \"square\", \"width\": 12, \"height\": 5 }, \"legend\": { \".\": \"plain\" },\n"
    "  \"tiles\": [\"............\", \"............\", \"............\", \"............\", \"............\"],\n"
    "  \"sides\": [ { \"id\": \"blue\", \"controller\": \"human\" }, { \"id\": \"red\", \"controller\": \"ai\" } ],\n"
    "  \"structures\": [ { \"type\": \"camp\", \"x\": 0, \"y\": 0, \"side\": \"blue\" }, { \"type\": \"camp\", \"x\": 11, \"y\": 4, \"side\": \"red\" } ],\n"
    "  \"units\": [ { \"type\": \"knight\", \"side\": \"blue\", \"x\": 1, \"y\": 2, \"id\": \"kn\", \"leader\": true },"
    " { \"type\": \"scout\", \"side\": \"blue\", \"x\": 1, \"y\": 3, \"id\": \"sc\" },"
    " { \"type\": \"dummy\", \"side\": \"red\", \"x\": 9, \"y\": 2, \"id\": \"foe\", \"leader\": true },"
    " { \"type\": \"dummy\", \"side\": \"red\", \"x\": 10, \"y\": 3, \"id\": \"foe2\" } ] }";

const char* kTwoHumanMap =
    "{ \"name\": \"T\", \"grid\": { \"type\": \"square\", \"width\": 12, \"height\": 5 }, \"legend\": { \".\": \"plain\" },\n"
    "  \"tiles\": [\"............\", \"............\", \"............\", \"............\", \"............\"],\n"
    "  \"sides\": [ { \"id\": \"blue\", \"controller\": \"human\" }, { \"id\": \"red\", \"controller\": \"human\" } ],\n"
    "  \"units\": [ { \"type\": \"knight\", \"side\": \"blue\", \"x\": 1, \"y\": 2, \"id\": \"kn\", \"leader\": true },"
    " { \"type\": \"dummy\", \"side\": \"red\", \"x\": 9, \"y\": 2, \"id\": \"foe\", \"leader\": true } ] }";

// The fixture's abilities without any charge key (for the games that must refuse the keys).
const char* kPlainAbilities =
    "{ \"abilities\": [\n"
    "  { \"id\": \"mend\", \"name\": \"Mend\", \"target\": \"ally\", \"range\": [1, 3], \"effect\": { \"heal\": 10 } },\n"
    "  { \"id\": \"hush\", \"name\": \"Hush\", \"target\": \"enemy\", \"range\": [1, 4], \"inflict\": { \"condition\": \"sleep\", \"turns\": 2 } },\n"
    "  { \"id\": \"dance\", \"name\": \"Dance\", \"target\": \"ally\", \"effect\": { \"refresh\": true } } ] }";

const char* kSpeedBlock = "\"turnOrder\": { \"mode\": \"speed\", \"stat\": \"spd\", \"threshold\": 100, \"preview\": 8 }";

int gold(Scene& s, int side) { return s.b().sides[(size_t)side].resources[(size_t)s.db().findResource("gold")]; }

// A condition on a unit as the rules keep it (the way the other tests put one there).
void give(Scene& s, const char* who, const char* condition, int turns) {
  ConditionState cs;
  cs.condition = s.db().findCondition(condition);
  cs.left = turns;
  s.unit(who).conditions.push_back(cs);
}

// The tick the clock stands at, counted from the start of the battle.
int absoluteTick(Scene& s) { return (s.b().round - 1) * s.db().rules.speed.roundTicks + s.b().clockTick; }

// The game's load message after one text of game.json was replaced.
std::string gameError(const char* name, const std::string& from, const std::string& to) {
  skilltest::Copy c(name, "speed-order");
  c.swap("game.json", from, to);
  return c.gameError();
}

// The same for another file; `plainGame` first takes the speed rule out (and the charge keys of the abilities).
std::string fileError(const char* name, const std::string& file, const std::string& from, const std::string& to, bool plainGame = false) {
  skilltest::Copy c(name, "speed-order");
  if (plainGame) {
    c.swap("game.json", std::string(kSpeedBlock) + ",", "");
    c.put("data/abilities.json", kPlainAbilities);
  }
  c.swap(file, from, to);
  return c.gameError();
}

}  // namespace

// ---- who has the turn ----

TEST(the_fastest_unit_has_the_first_turn_and_only_it_may_act) {
  Scene s("sp_start", plain(12, 5), kField);
  CHECK(s.b().speedGame());
  CHECK_STR(s.active(), "sc");  // speed 20 reaches the threshold of 100 at tick 5
  CHECK_EQ(s.b().clockTick, 5);
  CHECK_EQ(s.unit("sc").ct, 100);
  CHECK_EQ(s.unit("kn").ct, 50);
  CHECK_EQ(s.unit("foe").ct, 50);
  CHECK_EQ(s.b().activeSide, 0);  // the side of the unit that has the turn
  CHECK_EQ(s.b().round, 1);
  CHECK(s.b().mayActNow(s.unit("sc")));
  CHECK(!s.b().mayActNow(s.unit("kn")));
  CHECK_STR(s.refused(Action::move(s.id("kn"), s.at(2, 2))), "that unit cannot act now");  // a friend of the active unit does not get the turn either
  CHECK_STR(s.refused(Action::move(s.id("foe"), s.at(8, 2))), "that unit cannot act now");
  CHECK(s.go("sc", 3, 3));
  CHECK_EQ(s.count(EventType::UnitTurnBegan), 0);  // the scout still has its turn
}

TEST(the_start_says_round_one_the_side_and_the_unit) {
  Scene s("sp_start_events", plain(12, 5), kField);
  CHECK_EQ(s.count(EventType::RoundStarted), 1);
  CHECK_EQ(s.count(EventType::TurnStarted), 1);
  CHECK_EQ(s.count(EventType::UnitTurnBegan), 1);
  const Event* e = s.find(EventType::UnitTurnBegan);
  CHECK(e != nullptr);
  if (e) {
    CHECK_EQ(e->a, s.id("sc"));
    CHECK_EQ(e->b, 1);    // the round
    CHECK_EQ(e->c, 100);  // its charge
  }
}

TEST(units_take_turns_in_the_order_their_speed_gives) {
  Scene s("sp_order", plain(12, 5), kField);
  // Waiting costs 60: the scout (20 a tick) is ready again after 3 ticks; at tick 10 three units reach 100 together (ties go to the lower id).
  CHECK_STR(s.turns(6), "sc sc kn foe foe2 sc");
}

TEST(a_tie_goes_to_more_charge_then_more_speed_then_the_lower_unit_id) {
  const std::vector<U> units{{"brute", "blue", 1, 2, "br"}, {"dummy", "blue", 1, 3, "du"}, {"scout", "red", 9, 2, "sc"}, {"dummy", "red", 10, 3, "d2"}};
  {
    Scene s("sp_tie_speed", plain(12, 5), units);
    CHECK_STR(s.active(), "sc");
    s.unit("br").ct = 100;  // as ready as the dummy, but slower: the dummy goes first although the brute has the lower id
    s.unit("du").ct = 100;
    s.unit("d2").ct = 0;
    CHECK_STR(s.turns(3), "sc du br");
  }
  {
    Scene s("sp_tie_charge", plain(12, 5), units);
    s.unit("br").ct = 130;  // more charge beats everything else
    s.unit("du").ct = 100;
    s.unit("d2").ct = 0;
    CHECK_STR(s.turns(3), "sc br du");
  }
}

TEST(nothing_in_the_clock_uses_randomness) {
  auto trace = [](const char* name) {
    Scene s(name, plain(12, 5), kField);
    return s.turns(40);
  };
  CHECK_STR(trace("sp_det1"), trace("sp_det2"));
  Scene s("sp_det3", plain(12, 5), kField);
  const uint64_t before = s.b().rng.state;
  s.turns(40);
  CHECK(s.b().rng.state == before);  // waiting for 40 turns rolled no dice
}

// ---- what a turn costs ----

TEST(a_turn_costs_the_charge_by_how_much_of_it_was_used) {
  const std::vector<U> units{{"scout", "blue", 1, 2, "sc"}, {"dummy", "red", 2, 2, "foe"}, {"brute", "red", 11, 4, "br"}};
  // The charge `sc` pays for what `act` does: its charge before, plus the 20 a tick it gained meanwhile, minus what it has after. The brute (5 a
  // tick, far away) is the clock that says how many ticks went by.
  auto cost = [&](const char* name, const std::function<void(Scene&)>& act) {
    Scene s(name, plain(12, 5), units);
    CHECK_STR(s.active(), "sc");
    const int sc0 = s.unit("sc").ct, br0 = s.unit("br").ct;
    act(s);
    const int ticks = (s.unit("br").ct - br0) / 5;
    return sc0 + 20 * ticks - s.unit("sc").ct;
  };
  CHECK_EQ(cost("sp_cost_both", [](Scene& s) { s.go("sc", 2, 3); s.hit("sc", "foe", "sword"); }), 100);
  CHECK_EQ(cost("sp_cost_move", [](Scene& s) { s.go("sc", 2, 3); s.wait("sc"); }), 80);
  CHECK_EQ(cost("sp_cost_act", [](Scene& s) { s.hit("sc", "foe", "sword"); }), 80);
  CHECK_EQ(cost("sp_cost_none", [](Scene& s) { s.wait("sc"); }), 60);
  CHECK_EQ(cost("sp_cost_end", [](Scene& s) { s.w->apply(Action::endTurn()); }), 60);  // End turn is the same as Wait
}

TEST(a_unit_that_moved_and_acted_has_its_turn_ended_by_itself) {
  const std::vector<U> units{{"scout", "blue", 1, 2, "sc"}, {"dummy", "red", 3, 2, "foe"}, {"brute", "red", 11, 4, "br"}};
  Scene s("sp_auto_end", plain(12, 5), units);
  CHECK_STR(s.active(), "sc");
  CHECK(s.go("sc", 2, 2));
  CHECK_EQ(s.b().turnNo, 1);  // it has not acted yet: the turn goes on
  CHECK(s.hit("sc", "foe", "sword"));
  CHECK_EQ(s.b().turnNo, 2);  // the attack finished it: the clock ran on and the next turn began
}

TEST(the_costs_are_data) {
  Scene s("sp_costs_data", plain(12, 5), kField, false, [](skilltest::Copy& c) {
    c.swap("game.json", "\"preview\": 8 }", "\"preview\": 8, \"ctCost\": { \"both\": 90, \"one\": 70, \"none\": 50 } }");
  });
  CHECK_EQ(s.b().db->rules.speed.costNone, 50);
  CHECK_EQ(s.b().db->rules.speed.costOne, 70);
  CHECK_EQ(s.b().db->rules.speed.costBoth, 90);
  CHECK_EQ(s.b().ctCostOf(s.unit("sc")), 50);  // a plain Wait
  CHECK(s.go("sc", 2, 3));
  CHECK_EQ(s.b().ctCostOf(s.unit("sc")), 70);  // moved only
}

// ---- rounds ----

TEST(a_round_passes_every_ten_ticks_and_the_sides_get_their_income_once_per_round) {
  Scene s("sp_round", kCampMap);
  CHECK_EQ(s.b().db->rules.speed.roundTicks, 10);  // a tenth of the threshold
  CHECK_EQ(gold(s, 0), 5);  // round 1 began for every side: one income each
  CHECK_EQ(gold(s, 1), 5);
  CHECK_STR(s.active(), "sc");
  s.pass();  // the scout takes two turns before tick 10, the rest one: none of that earns more
  CHECK_STR(s.active(), "sc");
  CHECK_EQ(gold(s, 0), 5);
  CHECK_EQ(s.b().round, 1);
  CHECK(s.until("kn"));  // tick 10: the second round has begun
  CHECK_EQ(s.b().round, 2);
  CHECK_EQ(s.b().clockTick, 0);
  CHECK_EQ(gold(s, 0), 10);
  CHECK_EQ(gold(s, 1), 10);
}

TEST(the_round_events_carry_the_round_and_come_before_the_turn_that_follows) {
  Scene s("sp_round_events", kCampMap);
  int rounds = 0;
  int lastRound = 1;
  for (int i = 0; i < 40; ++i) {
    s.pass();
    for (const Event& e : s.w->events) {
      if (e.type == EventType::RoundStarted) {
        CHECK_EQ(e.a, lastRound + 1);
        lastRound = e.a;
        ++rounds;
      }
      if (e.type == EventType::UnitTurnBegan) CHECK_EQ(e.b, s.b().round);  // the unit that has the turn now began it in this round
    }
  }
  CHECK(rounds >= 3);
  CHECK_EQ(s.b().round, lastRound);
}

TEST(a_condition_counts_in_its_units_own_turns_not_in_rounds) {
  Scene s("sp_poison", plain(12, 5), kField);
  give(s, "sc", "poison", 3);  // 3 HP at the start of each of its turns; it lasts three of them
  CHECK_EQ(s.unit("sc").hp, 30);
  CHECK(s.until("kn"));  // the scout had one more turn by tick 10 (tick 8)
  CHECK_EQ(s.unit("sc").hp, 27);
  CHECK_EQ(s.unit("kn").hp, 30);  // nobody else was touched
  for (int i = 0; i < 40 && !s.unit("sc").conditions.empty(); ++i) s.pass();
  CHECK_EQ(s.unit("sc").hp, 21);  // three poisoned turns in all, then it wore off
  CHECK(s.unit("sc").conditions.empty());
}

TEST(a_stopped_unit_gains_no_charge_and_its_condition_counts_in_rounds) {
  Scene s("sp_stop", plain(12, 5), kField);
  give(s, "kn", "stop", 2);  // cannot act; the round count takes it off at the start of the third round
  CHECK_EQ(s.b().speedOf(s.unit("kn")), 0);
  CHECK_EQ(s.b().speedOf(s.unit("sc")), 20);
  bool acted = false;
  for (int i = 0; i < 60 && !acted; ++i) {
    if (s.active() == "kn") {
      acted = true;
      break;
    }
    if (s.b().round < 3) CHECK_EQ(s.unit("kn").ct, 50);  // frozen while stopped
    s.pass();
  }
  CHECK(acted);
  CHECK_EQ(s.b().round, 3);  // freed at the start of round 3 (tick 20), then 5 ticks of 10 to the threshold
  CHECK_EQ(s.b().clockTick, 5);
  CHECK_EQ(absoluteTick(s), 25);
  CHECK(s.unit("kn").conditions.empty());
}

TEST(a_unit_with_no_speed_never_has_a_turn_and_a_battle_nobody_can_play_ends_in_a_draw) {
  {
    const std::vector<U> units{{"knight", "blue", 1, 2, "kn"}, {"stiff", "blue", 1, 3, "st"}, {"dummy", "red", 9, 2, "foe"}};
    Scene s("sp_stiff", plain(12, 5), units);
    for (int i = 0; i < 100; ++i) {
      CHECK(s.active() != "st");
      s.pass();
    }
  }
  {
    const std::vector<U> units{{"stiff", "blue", 1, 2, "a"}, {"stiff", "red", 9, 2, "b"}};
    Scene s("sp_all_stiff", plain(12, 5), units);
    CHECK(s.b().over);
    CHECK_EQ(s.b().winnerTeam, -1);
  }
}

// ---- the story and the objectives count rounds ----

TEST(story_turn_events_fire_at_the_start_of_their_round_and_an_objective_can_count_rounds) {
  const char* map =
      "{ \"name\": \"T\", \"grid\": { \"type\": \"square\", \"width\": 12, \"height\": 5 }, \"legend\": { \".\": \"plain\" },\n"
      "  \"tiles\": [\"............\", \"............\", \"............\", \"............\", \"............\"],\n"
      "  \"sides\": [ { \"id\": \"blue\", \"controller\": \"human\" }, { \"id\": \"red\", \"controller\": \"ai\" } ],\n"
      "  \"units\": [ { \"type\": \"knight\", \"side\": \"blue\", \"x\": 1, \"y\": 2, \"id\": \"kn\", \"leader\": true },"
      " { \"type\": \"dummy\", \"side\": \"red\", \"x\": 9, \"y\": 2, \"id\": \"foe\", \"leader\": true } ],\n"
      "  \"objectives\": { \"win\": [ { \"survive\": 4 } ] },\n"
      "  \"events\": [ { \"id\": \"reinforce\", \"when\": { \"turn\": 3, \"side\": \"red\" },"
      " \"do\": [ { \"addUnit\": { \"type\": \"dummy\", \"side\": \"red\", \"x\": 10, \"y\": 4, \"id\": \"rf\" } } ] } ] }";
  Scene s("sp_story", map);
  CHECK_EQ(s.b().unitByStoryId("rf"), -1);
  int added = 0, roundOfAdd = 0;
  for (int i = 0; i < 80 && !s.b().over; ++i) {
    s.pass();
    for (const Event& e : s.w->events)
      if (e.type == EventType::UnitAdded) {
        ++added;
        roundOfAdd = s.b().round;
      }
  }
  CHECK_EQ(added, 1);                  // once, at the start of round 3 (the "turn": 3 event of the red side), not once per red unit turn
  CHECK_EQ(roundOfAdd, 3);
  CHECK(s.b().over);                   // "survive 4 rounds": the human team wins when round 4 has begun
  CHECK_EQ(s.b().round, 4);
  CHECK_EQ(s.b().winnerTeam, s.b().sides[0].team);
}

// ---- the skill that gives another turn ----

TEST(a_refresh_ability_fills_the_target_charge_so_it_acts_as_soon_as_the_order_reaches_it) {
  const std::vector<U> units{{"dancer", "blue", 1, 2, "da"}, {"brute", "blue", 1, 3, "br"}, {"dummy", "red", 9, 2, "foe"}, {"twin", "blue", 2, 2, "tw"}};
  Scene s("sp_dance", plain(12, 5), units);
  // Tick 10: dancer, foe and twin are ready (ties by id), the brute (5 a tick) has 50.
  CHECK_STR(s.active(), "da");
  CHECK_EQ(s.unit("br").ct, 50);
  CHECK_STR(s.refused(Action::useAbility(s.id("da"), s.ability("dance"), s.id("tw"))), "that unit is ready already");
  CHECK(s.cast("da", "dance", "br"));
  CHECK_EQ(s.unit("br").ct, 100);
  // Without the dance the brute would wait until tick 20. Now it is ready at once: after the units with more speed at the same charge.
  CHECK_STR(s.active(), "foe");
  CHECK_STR(s.turns(4), "foe tw br foe");
}

// ---- other sides ----

TEST(with_two_human_sides_the_acting_side_is_the_one_of_the_unit_that_has_the_turn) {
  Scene s("sp_hotseat", kTwoHumanMap);
  CHECK_STR(s.active(), "kn");  // the first of two units that are ready together
  CHECK_EQ(s.b().activeSide, 0);
  s.pass();
  CHECK_STR(s.active(), "foe");
  CHECK_EQ(s.b().activeSide, 1);
  CHECK(s.b().canAct(s.id("foe")));
  CHECK(!s.b().canAct(s.id("kn")));
  CHECK(s.go("foe", 8, 2));  // the red person moves their unit now
  CHECK_EQ(s.count(EventType::TurnStarted), 0);  // (no new turn began)
  s.wait("foe");
  CHECK_EQ(s.count(EventType::TurnStarted), 1);  // the turn passed to the blue side again: a banner and its music
}

TEST(a_game_without_the_speed_rule_plays_side_by_side_as_ever) {
  areatest::Scene s("sp_sides", plain(8, 3), {{"lancer", "blue", 1, 1, "a"}, {"lancer", "blue", 2, 1, "b"}, {"dummy", "red", 6, 1, "c"}});
  CHECK(!s.b().speedGame());
  CHECK_EQ(s.b().activeUnit, -1);
  CHECK(s.b().mayActNow(s.unit("a")));  // every unit of the active side
  CHECK(s.b().mayActNow(s.unit("b")));
  CHECK(!s.b().mayActNow(s.unit("c")));
  CHECK_EQ(s.b().speedOf(s.unit("a")), 0);
  std::vector<TurnEntry> order;
  s.b().turnPreview(5, order);
  CHECK(order.empty());
}

// ---- the loader ----

TEST(the_speed_rule_is_read_and_refused_in_plain_words) {
  const std::string block = kSpeedBlock;
  CHECK_STR(gameError("sp_ld_ok", block, block), "");
  CHECK(gameError("sp_ld_nostat", block, "\"turnOrder\": \"speed\"")
            .find("a speed game needs a stat for the clock: add a stat \"speed\" or name one with { \"mode\": \"speed\", \"stat\": \"<stat id>\" }") != std::string::npos);
  {
    skilltest::Copy c("sp_ld_short", "speed-order");  // "speed" alone works when the game has a stat called speed
    c.swap("game.json", "{ \"id\": \"spd\", \"name\": \"Speed\" }", "{ \"id\": \"speed\", \"name\": \"Speed\" }");
    c.swap("game.json", block, "\"turnOrder\": \"speed\"");
    std::string units = c.read("data/units.json");
    for (size_t at; (at = units.find("\"spd\"")) != std::string::npos;) units.replace(at, 5, "\"speed\"");
    c.put("data/units.json", units);
    CHECK_STR(c.gameError(), "");
  }
  CHECK(gameError("sp_ld_stat", "\"stat\": \"spd\"", "\"stat\": \"nope\"").find("rules.turnOrder: \"stat\" names unknown stat \"nope\"") != std::string::npos);
  CHECK(gameError("sp_ld_mode", block, "\"turnOrder\": \"fast\"").find("must be \"sides\"") != std::string::npos);
  CHECK(gameError("sp_ld_key", "\"preview\": 8", "\"preview\": 8, \"speedy\": 1").find("unknown key \"speedy\"") != std::string::npos);
  CHECK(gameError("sp_ld_cost", "\"preview\": 8 }", "\"preview\": 8, \"ctCost\": { \"both\": 100, \"one\": 0, \"none\": 60 } }").find("every cost must be at least 1") != std::string::npos);
  CHECK(gameError("sp_ld_cost_key", "\"preview\": 8 }", "\"preview\": 8, \"ctCost\": { \"all\": 100 } }").find("unknown key \"all\"") != std::string::npos);
  CHECK(gameError("sp_ld_thr", "\"threshold\": 100", "\"threshold\": 0").find("\"threshold\" must be a whole number from 1") != std::string::npos);
  CHECK(gameError("sp_ld_ap", "\"economy\": { \"enabled\": true },", "\"economy\": { \"enabled\": true }, \"actionPoints\": { \"resource\": \"gold\" },")
            .find("cannot be combined with \"actionPoints\"") != std::string::npos);
  CHECK(gameError("sp_ld_prod", "\"economy\": { \"enabled\": true },", "\"economy\": { \"enabled\": true }, \"production\": { \"enabled\": true },")
            .find("cannot be combined with \"production\"") != std::string::npos);
}

TEST(charge_keys_need_the_speed_rule_and_make_sense) {
  // Without the speed rule the abilities' keys are refused first (abilities load before weapons), then the weapons'.
  {
    skilltest::Copy c("sp_ch_off_ab", "speed-order");
    c.swap("game.json", std::string(kSpeedBlock) + ",", "");
    const std::string err = c.gameError();
    CHECK(err.find("data/abilities.json: ability \"mend\"") != std::string::npos);
    CHECK(err.find("need \"rules\": { \"turnOrder\": \"speed\" } in game.json") != std::string::npos);
  }
  {
    const std::string err = fileError("sp_ch_off_w", "data/weapons.json", "\"chargeSpeed\": 25", "\"chargeSpeed\": 25", true);
    CHECK(err.find("data/weapons.json: weapon \"fire\"") != std::string::npos);
  }
  CHECK(fileError("sp_ch_range", "data/weapons.json", "\"chargeSpeed\": 25", "\"chargeSpeed\": 0").find("\"chargeSpeed\" must be a whole number from 1 to 1000") != std::string::npos);
  CHECK(fileError("sp_ch_aim", "data/weapons.json", "\"chargeSpeed\": 25,", "\"chargeSpeed\": 25, \"chargeTargets\": \"all\",").find("\"chargeTargets\" must be \"unit\"") != std::string::npos);
  CHECK(fileError("sp_ch_cond", "data/weapons.json", "[\"sleep\"]", "[\"nap\"]").find("unknown condition \"nap\"") != std::string::npos);
  CHECK(fileError("sp_ch_alone", "data/weapons.json", "\"chargeSpeed\": 20 }", "\"interruptedBy\": [\"sleep\"] }").find("go with \"chargeSpeed\"") != std::string::npos);
  CHECK(fileError("sp_ch_free", "data/abilities.json", "\"chargeSpeed\": 20 }", "\"chargeSpeed\": 20, \"cost\": \"free\" }").find("needs \"cost\": \"action\"") != std::string::npos);
  CHECK_STR(fileError("sp_ch_fine", "data/weapons.json", "\"chargeSpeed\": 25", "\"chargeSpeed\": 25"), "");
}
