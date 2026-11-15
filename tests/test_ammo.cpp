// Ammunition (docs/ECONOMY.md "Repair and supply"): one per attack (initiating or countering, never per strike),
// a weapon at 0 is not offered (the next one is used, or the unit cannot attack / counter), and every query that
// lists attacks agrees.
#include "supply_fixture.h"

using namespace ote;
using namespace suptest;

namespace {

// Blue ranger (0,1) next to red ranger (1,1); every attack strikes twice.
struct Duel : SupplyWorld {
  Duel() : SupplyWorld(field(), {{"ranger", 0, 0, 1, false}, {"ranger", 1, 1, 1, false}}) { db.rules.fStrikes = "2"; }
};

bool offers(const Battle& b, int unit, int weapon, int target) {
  std::vector<AttackOption> opts;
  b.attackOptions(unit, b.units[(size_t)unit].cell, opts);
  for (const AttackOption& o : opts)
    if (o.weapon == weapon && o.target == target) return true;
  return false;
}

}  // namespace

TEST(ammo_units_start_with_a_full_load_of_the_weapons_that_have_one) {
  Duel w;
  CHECK(w.begin());
  CHECK_EQ(w.battle.ammo(0, Gun), 3);
  CHECK_EQ(w.battle.maxAmmo(0, Gun), 3);
  CHECK_EQ(w.battle.ammo(0, 0), -1);       // the sword is unlimited
  CHECK_EQ(w.battle.maxAmmo(0, 0), -1);
  CHECK_EQ(w.battle.ammo(0, 2), -1);       // not this unit's weapon
  CHECK_EQ(w.battle.ammo(99, Gun), -1);
  // A soldier has no weapon with ammunition: nothing to track.
  SupplyWorld s(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 6, 2, false}});
  CHECK(s.begin());
  CHECK(s.battle.units[0].ammo.empty());
  CHECK_EQ(s.battle.ammo(0, 0), -1);
}

TEST(ammo_an_attack_costs_one_however_many_strikes) {
  Duel w;
  CHECK(w.begin());
  CHECK(w.act(Action::attack(0, 1, Gun)));
  CHECK_EQ(w.count(EventType::Attack), 4);        // two strikes each way
  CHECK_EQ(w.battle.ammo(0, Gun), 2);             // one spent by the attacker
  CHECK_EQ(w.battle.ammo(1, Gun), 2);             // one by the counterattack
}

TEST(ammo_a_counter_that_never_happens_costs_nothing) {
  Duel w;
  CHECK(w.begin());
  w.battle.units[1].hp = 1;                       // the first strike kills
  CHECK(w.act(Action::attack(0, 1, Gun)));
  CHECK(!w.battle.units[1].alive);
  CHECK_EQ(w.battle.ammo(0, Gun), 2);
  CHECK_EQ(w.battle.ammo(1, Gun), 3);
}

TEST(ammo_the_sword_does_not_count) {
  Duel w;
  CHECK(w.begin());
  CHECK(w.act(Action::attack(0, 1, 0)));          // sword
  CHECK_EQ(w.battle.ammo(0, Gun), 3);
  CHECK_EQ(w.battle.ammo(1, Gun), 2);             // the counter prefers the first usable weapon: the gun
}

TEST(ammo_an_empty_weapon_is_not_offered_and_the_next_one_is_used) {
  Duel w;
  CHECK(w.begin());
  CHECK(offers(w.battle, 0, Gun, 1) && offers(w.battle, 0, 0, 1));
  w.battle.units[0].ammo[0] = 0;                  // the gun is empty
  std::vector<int> weapons;
  w.battle.weaponsOf(w.battle.units[0], weapons);
  CHECK_EQ(weapons.size(), size_t(1));
  CHECK_EQ(weapons[0], 0);
  CHECK(!offers(w.battle, 0, Gun, 1));
  CHECK(offers(w.battle, 0, 0, 1));
  Forecast f;
  CHECK(!w.battle.forecast(0, 1, Gun, w.battle.units[0].cell, f));
  CHECK(w.battle.forecast(0, 1, 0, w.battle.units[0].cell, f));
  CHECK_STR(w.refused(Action::attack(0, 1, Gun)), "that attack is not possible");
  CHECK(w.act(Action::attack(0, 1, 0)));
  CHECK_EQ(w.battle.ammo(0, Gun), 0);             // nothing was spent: it was empty already
}

TEST(ammo_an_empty_defender_counters_with_its_next_weapon) {
  Duel w;
  CHECK(w.begin());
  w.battle.units[1].ammo[0] = 0;
  Forecast f;
  CHECK(w.battle.forecast(0, 1, Gun, w.battle.units[0].cell, f));
  CHECK(f.counter.can);
  CHECK_EQ(f.counter.weapon, 0);                  // the sword
  w.events.clear();
  CHECK(w.act(Action::attack(0, 1, Gun)));
  for (const Event& e : w.events)
    if (e.type == EventType::Attack && (e.flags & kCounter)) CHECK_EQ(e.c, 0);
  CHECK_EQ(w.battle.ammo(1, Gun), 0);
}

TEST(ammo_a_unit_with_only_an_empty_weapon_cannot_attack_or_counter) {
  SupplyWorld w(field(), {{"tank", 0, 0, 1, false}, {"tank", 1, 1, 1, false}});
  CHECK(w.begin());
  CHECK_EQ(w.battle.ammo(0, 2), 2);
  CHECK_EQ(w.battle.maxAmmo(0, 2), 2);
  w.battle.units[1].ammo[0] = 0;
  Forecast f;
  CHECK(w.battle.forecast(0, 1, 2, w.battle.units[0].cell, f));
  CHECK(!f.counter.can);                          // the red tank cannot answer
  CHECK(w.act(Action::attack(0, 1, 2)));
  CHECK_EQ(w.battle.ammo(0, 2), 1);
  for (const Event& e : w.events) CHECK(!(e.type == EventType::Attack && (e.flags & kCounter)));
  // Now the attacker's own is spent down to 0: no attack options at all.
  CHECK(w.nextTurn());
  w.battle.units[0].ammo[0] = 0;
  std::vector<AttackOption> opts;
  w.battle.attackOptions(0, w.battle.units[0].cell, opts);
  CHECK(opts.empty());
}

TEST(ammo_attack_range_queries_ignore_an_empty_weapons_range) {
  // The gun reaches 2 squares, the sword 1: with the gun empty a target two squares away is out of reach.
  SupplyWorld w(field(), {{"ranger", 0, 0, 1, false}, {"soldier", 1, 2, 1, false}});
  CHECK(w.begin());
  CHECK(offers(w.battle, 0, Gun, 1));
  w.battle.units[0].ammo[0] = 0;
  std::vector<AttackOption> opts;
  w.battle.attackOptions(0, w.battle.units[0].cell, opts);
  CHECK(opts.empty());
}

TEST(ammo_carried_weapons_use_item_uses_and_no_ammunition) {
  SupplyWorld w(field(), {{"ranger", 0, 0, 1, false}, {"soldier", 1, 1, 1, false}});
  fixture::addItems(w, true);
  fixture::carry(w, 0, {fixture::items::SwordItem});
  CHECK(w.begin());
  CHECK(w.battle.units[0].ammo.empty());          // nothing is tracked for carried weapons
  CHECK_EQ(w.battle.ammo(0, Gun), -1);
  CHECK(w.act(Action::attack(0, 1, 0)));
  CHECK_EQ(w.battle.units[0].items[0].uses, 1);   // the item wore, as in docs/ITEMS.md
}

TEST(ammo_spending_is_replayed_the_same_from_a_copy) {
  Duel a;
  CHECK(a.begin());
  Battle copy = a.battle;
  std::vector<Event> e;
  CHECK(copy.apply(Action::attack(0, 1, Gun), e));
  CHECK(a.act(Action::attack(0, 1, Gun)));
  CHECK_EQ(copy.units[0].ammo[0], a.battle.units[0].ammo[0]);
  CHECK_EQ(copy.units[1].ammo[0], a.battle.units[1].ammo[0]);
}
