// Docs/TURN_ORDER.md "Charged actions": declaring, going off before the units of the same tick, following a unit or hitting a square, a caster that
// falls or is interrupted, the charging unit's own turns, the preview and its forecast of a charged action. Fixture tests/fixtures/speed-order.
//
// Timeline used by most tests (every unit 10 a tick, so the first three act together at tick 10 by unit id): "fire" (25 a tick) declared at tick 10
// goes off at tick 14, "bolt" and "mend" (20) at tick 15, "hush" (25) at tick 14.
#include "speed_fixture.h"

using namespace ote;
using speedtest::Scene;
using speedtest::plain;
using speedtest::U;

namespace {

// A mage with two enemies next to each other, aimed at by "fire" from where it stands (distance 4 and 5).
const std::vector<U> kClump{{"mage", "blue", 3, 2, "mg"}, {"dummy", "red", 7, 2, "foe"}, {"dummy", "red", 8, 2, "foe2"}};

bool hasEvent(Scene& s, EventType t) { return s.find(t) != nullptr; }

// The ChargeCancelled reason of the last action's events (-1 = none).
int cancelReason(Scene& s) {
  const Event* e = s.find(EventType::ChargeCancelled);
  return e ? e->d : -1;
}

// The units the last action's Attack events hit (not counters), as story ids in the order struck.
std::string struck(Scene& s) {
  std::string out;
  for (const Event& e : s.w->events)
    if (e.type == EventType::Attack && !(e.flags & kCounter)) out += (out.empty() ? "" : " ") + s.b().units[(size_t)e.b].storyId;
  return out;
}

}  // namespace

// ---- declaring ----

TEST(a_charged_weapon_is_declared_not_fired_and_ends_the_turn) {
  Scene s("ch_declare", plain(12, 5), kClump);
  CHECK_STR(s.active(), "mg");
  CHECK(s.shoot("mg", "fire", 7, 2));
  const Event* e = s.find(EventType::ActionCharging);
  CHECK(e != nullptr);
  if (e) {
    CHECK_EQ(e->a, s.id("mg"));
    CHECK_EQ(e->b, 1);  // a weapon
    CHECK_EQ(e->c, s.weapon("fire"));
    CHECK_EQ(e->d, s.at(7, 2));  // the square aimed at
    CHECK_EQ(e->data[0], -1);    // an area weapon aims at the square: it follows nobody
    CHECK_EQ(e->data[1], 25);
  }
  CHECK(!hasEvent(s, EventType::Attack));  // nothing was hit yet
  CHECK_EQ(s.unit("foe").hp, 40);
  CHECK_EQ(s.b().pending.size(), (size_t)1);
  CHECK_EQ(s.b().pending[0].counter, 0);
  CHECK(s.b().charging(s.id("mg")));
  CHECK_STR(s.active(), "foe");  // the mage's turn is over
  CHECK_EQ(s.b().pending[0].serial, 1);
}

TEST(a_charged_action_goes_off_when_its_counter_fills_and_hits_what_is_there) {
  Scene s("ch_resolve", plain(12, 5), kClump);
  s.shoot("mg", "fire", 7, 2);
  CHECK(s.pass());  // foe waits
  CHECK(!hasEvent(s, EventType::ChargeResolved));
  CHECK(s.pass());  // foe2 waits: the clock runs 4 ticks and the counter (25 a tick) fills at tick 14, before anybody is ready again
  const Event* e = s.find(EventType::ChargeResolved);
  CHECK(e != nullptr);
  if (e) {
    CHECK_EQ(e->a, s.id("mg"));
    CHECK_EQ(e->d, s.at(7, 2));
  }
  CHECK_STR(struck(s), "foe foe2");
  CHECK_EQ(s.unit("foe").hp, 31);  // 5 + 6 - 2 = 9 each
  CHECK_EQ(s.unit("foe2").hp, 31);
  CHECK(s.b().pending.empty());
  CHECK(!s.b().charging(s.id("mg")));
}

TEST(a_charged_action_goes_off_before_a_unit_that_is_ready_at_the_same_tick) {
  // Make the mage's fire (4 ticks) and the foe's turn arrive at the same tick: the foe is ready again 4 ticks after tick 10 when its cost is 40.
  Scene s("ch_before_unit", plain(12, 5), kClump, false, [](skilltest::Copy& c) {
    c.swap("game.json", "\"preview\": 8 }", "\"preview\": 8, \"ctCost\": { \"both\": 100, \"one\": 100, \"none\": 40 } }");
  });
  s.shoot("mg", "fire", 7, 2);   // the mage pays 100: it is ready again after 10 ticks
  s.pass();                      // foe: 40, ready again at tick 14 (the same tick the fire goes off)
  s.pass();                      // foe2: 40, the same
  // The clock stops at tick 14: the fire went off first, in the events of foe2's turn end, then the foe's turn began.
  std::string order;
  for (const Event& e : s.w->events) {
    if (e.type == EventType::ChargeResolved) order += "fire ";
    if (e.type == EventType::UnitTurnBegan) order += s.b().units[(size_t)e.a].storyId + " ";
  }
  CHECK_STR(order, "fire foe ");
}

TEST(a_square_target_hits_whoever_is_there_when_it_goes_off) {
  Scene s("ch_cell", plain(12, 5), kClump);
  s.shoot("mg", "fire", 7, 2);
  CHECK(s.go("foe", 7, 4));  // walks out of the blast (the area is a radius of 1 around 7,2)
  s.wait("foe");
  s.pass();  // foe2 waits in the blast
  CHECK_STR(struck(s), "foe2");
  CHECK_EQ(s.unit("foe").hp, 40);
  CHECK_EQ(s.unit("foe2").hp, 31);
}

TEST(a_charged_weapon_on_a_unit_follows_it) {
  const std::vector<U> units{{"mage", "blue", 3, 2, "mg"}, {"dummy", "red", 6, 2, "foe"}};
  Scene s("ch_follow", plain(12, 5), units);
  CHECK(s.hit("mg", "foe", "bolt"));
  const Event* e = s.find(EventType::ActionCharging);
  CHECK(e != nullptr);
  if (e) CHECK_EQ(e->data[0], s.id("foe"));  // it follows the foe
  CHECK(s.b().pending[0].follow);
  CHECK(s.go("foe", 5, 3));  // still in range (3): the bolt goes with it
  s.wait("foe");
  CHECK_STR(struck(s), "foe");
  CHECK_EQ(s.unit("foe").hp, 30);  // 5 + 7 - 2 = 10
  CHECK_EQ(s.find(EventType::ChargeResolved)->d, s.at(5, 3));  // where it was when the bolt struck
}

TEST(a_charged_action_that_cannot_reach_its_target_when_it_goes_off_fizzles) {
  const std::vector<U> units{{"mage", "blue", 3, 2, "mg"}, {"dummy", "red", 6, 2, "foe"}};
  Scene s("ch_fizzle", plain(12, 5), units);
  s.hit("mg", "foe", "bolt");
  CHECK(s.go("foe", 9, 2));  // 6 squares away: beyond the bolt's range of 4
  s.wait("foe");
  CHECK(!hasEvent(s, EventType::ChargeResolved));
  CHECK_EQ(cancelReason(s), kChargeFizzled);
  CHECK_EQ(s.unit("foe").hp, 40);
  CHECK(s.b().pending.empty());
}

TEST(a_charged_ability_follows_its_target_and_is_paid_when_declared) {
  const std::vector<U> units{{"mage", "blue", 3, 2, "mg"}, {"dancer", "blue", 4, 2, "da"}, {"dummy", "red", 9, 2, "foe"}};
  Scene s("ch_mend", plain(12, 5), units);
  s.unit("da").hp = 5;
  CHECK(s.cast("mg", "mend", "da"));
  CHECK_EQ(s.find(EventType::ActionCharging)->b, 0);  // an ability
  CHECK_EQ(s.unit("da").hp, 5);                        // nothing healed yet
  CHECK_EQ(s.unit("mg").abilityUses.size(), (size_t)1);  // its use is counted from the declaration
  CHECK(s.go("da", 4, 3));
  s.wait("da");
  s.pass();  // the foe waits; the clock runs 5 ticks and the heal goes off
  CHECK(hasEvent(s, EventType::ChargeResolved));
  CHECK(hasEvent(s, EventType::AbilityUsed));
  CHECK_EQ(s.unit("da").hp, 15);
}

TEST(a_charged_ability_aimed_at_a_square_takes_whoever_has_come_into_it) {
  const std::vector<U> units{{"mage", "blue", 3, 2, "mg"}, {"dummy", "red", 6, 2, "foe"}, {"dummy", "red", 7, 3, "foe2"}};
  Scene s("ch_hush", plain(12, 5), units);
  CHECK(s.cast("mg", "hush", "foe"));
  CHECK(!s.b().pending[0].follow);  // "chargeTargets": "cell"
  CHECK(s.go("foe", 6, 4));         // the foe leaves
  s.wait("foe");
  CHECK(s.go("foe2", 6, 2));        // another one takes its place
  s.wait("foe2");
  CHECK(s.b().hasCondition(s.unit("foe2"), s.db().findCondition("sleep")));
  CHECK(!s.b().hasCondition(s.unit("foe"), s.db().findCondition("sleep")));
}

TEST(a_charged_weapon_is_never_the_answer_to_a_counter_attack) {
  const std::vector<U> units{{"mage", "blue", 3, 2, "mg"}, {"dummy", "red", 4, 2, "foe"}};
  Scene s("ch_nocounter", plain(12, 5), units, false, [](skilltest::Copy& c) {
    c.swap("data/units.json", "\"weapons\": [\"fire\", \"bolt\", \"sword\"]", "\"weapons\": [\"fire\", \"bolt\"]");
  });
  s.pass();  // the mage waits
  CHECK(s.hit("foe", "mg", "sword"));
  bool counter = false;
  for (const Event& e : s.w->events) counter = counter || (e.type == EventType::Attack && (e.flags & kCounter));
  CHECK(!counter);
}

TEST(facing_works_in_a_speed_game_and_declaring_turns_the_caster_to_its_aim) {
  // The mage starts looking at the nearest foe (the dummy to the north, 2 squares); the fire is aimed east: declaring turns it.
  const std::vector<U> units{{"mage", "blue", 3, 2, "mg"}, {"dummy", "red", 3, 0, "near"}, {"dummy", "red", 7, 2, "foe"}};
  Scene s("ch_facing", plain(12, 5), units, false, [](skilltest::Copy& c) {
    c.swap("game.json", "\"items\": { \"weapons\": \"fixed\", \"inventorySize\": 3 } }", "\"items\": { \"weapons\": \"fixed\", \"inventorySize\": 3 }, \"facing\": {} }");
  });
  CHECK(s.b().facingGame());
  const int before = s.unit("mg").facing;
  CHECK(before >= 0);
  CHECK(s.shoot("mg", "fire", 7, 2));
  const int east = 1;  // the square directions are numbered north, east, south, west
  CHECK(east != before);
  CHECK_EQ(s.unit("mg").facing, east);
  CHECK(hasEvent(s, EventType::UnitFaced));
}

// ---- when it does not go off ----

TEST(a_caster_that_falls_takes_its_action_with_it) {
  const std::vector<U> units{{"mage", "blue", 3, 2, "mg"}, {"dummy", "red", 7, 2, "foe"}};
  Scene s("ch_dead", plain(12, 5), units, false, [](skilltest::Copy& c) {
    c.swap("data/units.json", "{ \"hp\": 20, \"str\": 5, \"def\": 1, \"mov\": 3, \"spd\": 10 }, \"weapons\": [\"fire\"", "{ \"hp\": 5, \"str\": 5, \"def\": 1, \"mov\": 3, \"spd\": 10 }, \"weapons\": [\"fire\"");
  });
  CHECK(s.shoot("mg", "fire", 7, 2));
  CHECK(s.go("foe", 4, 2));
  CHECK(s.hit("foe", "mg", "sword"));  // 3 + 4 - 1 = 6: the mage (5 HP) falls
  CHECK(!s.unit("mg").alive);
  CHECK_EQ(cancelReason(s), kChargeDefeated);
  CHECK(s.b().pending.empty());
}

TEST(a_condition_named_by_interruptedBy_cancels_the_action) {
  // The foe's "hush" (here 50 a tick = 2 ticks) puts the mage to sleep before its fire (4 ticks) is ready; "sleep" is in the fire's interruptedBy.
  const std::vector<U> units{{"mage", "blue", 3, 2, "mg"}, {"mage", "red", 7, 2, "rm"}};
  Scene s("ch_interrupt", plain(12, 5), units, false, [](skilltest::Copy& c) {
    c.swap("data/abilities.json", "\"chargeSpeed\": 25, \"chargeTargets\"", "\"chargeSpeed\": 50, \"chargeTargets\"");
  });
  CHECK(s.shoot("mg", "fire", 7, 2));
  CHECK(s.b().charging(s.id("mg")));
  CHECK(s.cast("rm", "hush", "mg"));  // the clock runs on inside this action: the hush goes off at tick 12
  bool applied = false, cancelled = false;
  for (const Event& e : s.w->events) {
    if (e.type == EventType::ConditionApplied && e.a == s.id("mg")) applied = true;
    if (e.type == EventType::ChargeCancelled && e.a == s.id("mg")) cancelled = applied && e.d == kChargeInterrupted;  // the sleep came first
  }
  CHECK(cancelled);
  CHECK(!s.b().charging(s.id("mg")));
  CHECK_EQ(s.unit("rm").hp, 20);  // and the fire never went off
}

// ---- the charging unit's own turn ----

TEST(a_unit_that_is_charging_may_move_but_not_act_until_it_gives_the_charge_up) {
  const std::vector<U> units{{"mage", "blue", 3, 2, "mg"}, {"dummy", "red", 7, 2, "foe"}};
  Scene s("ch_own_turn", plain(12, 5), units, false, [](skilltest::Copy& c) {
    c.swap("data/weapons.json", "\"chargeSpeed\": 25", "\"chargeSpeed\": 5");  // 20 ticks: the mage is ready again first
  });
  s.shoot("mg", "fire", 7, 2);
  CHECK(s.until("mg"));
  CHECK(s.b().charging(s.id("mg")));  // still on its way
  const std::string charging = "that unit is charging an action: it may only move, or give the charge up";
  CHECK_STR(s.refused(Action::attack(s.id("mg"), s.id("foe"), s.weapon("sword"))), charging);
  CHECK_STR(s.refused(Action::useAbility(s.id("mg"), s.ability("mend"), s.id("mg"))), charging);
  CHECK_STR(s.refused(Action::capture(s.id("mg"))), charging);
  CHECK(s.go("mg", 4, 2));  // moving is fine: the fire is aimed at a square and will be fired from here
  CHECK_STR(s.active(), "mg");
  CHECK(s.b().charging(s.id("mg")));
  s.wait("mg");
  CHECK(s.b().charging(s.id("mg")));
}

TEST(giving_the_charge_up_frees_the_unit_to_act_and_wastes_what_it_paid) {
  const std::vector<U> units{{"mage", "blue", 3, 2, "mg"}, {"dummy", "red", 7, 2, "foe"}, {"dummy", "red", 4, 2, "near"}};
  Scene s("ch_cancel", plain(12, 5), units, false, [](skilltest::Copy& c) { c.swap("data/weapons.json", "\"chargeSpeed\": 25", "\"chargeSpeed\": 5"); });
  s.shoot("mg", "fire", 7, 2);
  CHECK(s.until("mg"));
  CHECK(s.b().validate(Action::cancelCharge(s.id("mg"))));
  CHECK(s.w->apply(Action::cancelCharge(s.id("mg"))));
  CHECK_EQ(cancelReason(s), kChargeByCaster);
  CHECK(!s.b().charging(s.id("mg")));
  CHECK_STR(s.active(), "mg");  // the turn goes on
  CHECK(s.hit("mg", "near", "sword"));  // and it can attack now
  CHECK(!s.b().validate(Action::cancelCharge(s.id("mg"))));
}

// ---- the preview ----

namespace {

// The sequence the clock then really produces, in the preview's words: the unit that has the turn, then each turn that begins and each charged action
// that goes off ("mg*"). Every unit waits.
std::string played(Scene& s, int n) {
  std::string out = s.active();
  int entries = 1;
  for (int guard = 0; guard < 200 && entries < n && !s.b().over; ++guard) {
    s.pass();
    for (const Event& e : s.w->events) {
      if (entries >= n) break;
      if (e.type == EventType::UnitTurnBegan) { out += " " + s.b().units[(size_t)e.a].storyId; ++entries; }
      if (e.type == EventType::ChargeResolved) { out += " " + s.b().units[(size_t)e.a].storyId + "*"; ++entries; }
    }
  }
  return out;
}

void equalCosts(skilltest::Copy& c) {
  c.swap("game.json", "\"preview\": 8 }", "\"preview\": 8, \"ctCost\": { \"both\": 100, \"one\": 100, \"none\": 100 } }");
}

const std::vector<U> kMix{{"mage", "blue", 3, 2, "mg"}, {"scout", "blue", 1, 3, "sc"}, {"dummy", "red", 7, 2, "foe"}, {"dummy", "red", 8, 2, "foe2"}, {"brute", "red", 9, 4, "br"}};

}  // namespace

TEST(the_preview_is_what_then_happens_when_every_turn_costs_the_same) {
  Scene a("ch_prev_a", plain(12, 5), kMix, false, equalCosts);
  Scene b("ch_prev_b", plain(12, 5), kMix, false, equalCosts);
  const std::string forecast = a.preview(12);
  CHECK_STR(forecast.substr(0, 2), "sc");
  CHECK_STR(played(b, 12), forecast);
}

TEST(the_preview_shows_a_charged_action_and_equals_what_then_happens) {
  Scene a("ch_prev_c", plain(12, 5), kMix, false, equalCosts);
  Scene b("ch_prev_d", plain(12, 5), kMix, false, equalCosts);
  for (Scene* s : {&a, &b}) {
    CHECK(s->until("mg"));
    CHECK(s->shoot("mg", "fire", 7, 2));
  }
  const std::string forecast = a.preview(10);
  CHECK(forecast.find("mg*") != std::string::npos);  // the pending action is in the order
  CHECK_STR(played(b, 10), forecast);
}

TEST(the_forecast_of_a_charged_action_is_where_it_then_lands_in_the_order) {
  Scene a("ch_fore_a", plain(12, 5), kMix, false, equalCosts);
  CHECK(a.until("mg"));
  int ticks = 0;
  const int index = a.b().chargeForecast(25, 12, &ticks);  // the order, the turn now first
  CHECK(index > 0);
  CHECK_EQ(ticks, 4);  // 25 a tick to 100
  CHECK(a.shoot("mg", "fire", 7, 2));
  std::vector<TurnEntry> order;
  a.b().turnPreview(12, order);
  int at = -1;
  for (size_t i = 0; i < order.size(); ++i)
    if (order[i].charged) at = (int)i;
  CHECK_EQ(at, index - 1);  // (the mage's own turn has left the order by now)
  CHECK_EQ(order[(size_t)at].ticks, 4);
  // A slower or faster action lands later or sooner.
  Scene b("ch_fore_b", plain(12, 5), kMix, false, equalCosts);
  CHECK(b.until("mg"));
  CHECK(b.b().chargeForecast(50, 12) <= b.b().chargeForecast(25, 12));
  const int slow = b.b().chargeForecast(5, 12, &ticks);  // 20 ticks: later in the order, or beyond what the preview shows
  CHECK(slow < 0 || slow > index);
  if (slow >= 0) CHECK_EQ(ticks, 20);
}

TEST(the_preview_changes_nothing) {
  Scene s("ch_prev_pure", plain(12, 5), kMix);
  const uint64_t rng = s.b().rng.state;
  std::vector<TurnEntry> order;
  s.b().turnPreview(30, order);
  CHECK_EQ(order.size(), (size_t)30);
  CHECK(s.b().rng.state == rng);
  CHECK_EQ(s.unit("foe").ct, 50);
  CHECK_EQ(s.b().clockTick, 5);
}
