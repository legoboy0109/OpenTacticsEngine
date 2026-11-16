// Combat pipeline (docs/SRPG.md step F2): the order of strikes (strikeFirst, multiStrike) and the counter rules
// (the answering weapon, counterAnyRange, noCounter), handed in through Battle::testMods.
#include "srpg_combat_fixture.h"

using namespace ote;
using namespace combattest;

namespace {

// A fight's order as text: "A2 D1 ..." = the attacker twice in a row, then the defender once.
std::string orderText(const Battle& b, const Forecast& f) {
  std::vector<StrikeStep> steps;
  b.strikeOrder(f, steps);
  std::string s;
  for (const StrikeStep& st : steps) s += std::string(s.empty() ? "" : " ") + (st.counter ? "D" : "A") + std::to_string(st.count);
  return s;
}

Forecast shape(int strikesA, int multiA, int strikesD, int multiD, bool defenderFirst) {
  Forecast f;
  f.attack.can = true;
  f.attack.strikes = strikesA;
  f.attack.multi = multiA;
  f.counter.can = strikesD > 0;
  f.counter.strikes = strikesD;
  f.counter.multi = multiD;
  f.defenderFirst = defenderFirst;
  return f;
}

}  // namespace

TEST(combat_order_rounds_and_strikes_in_a_row) {
  Duel d;
  CHECK(d.begin());
  const Battle& b = d.w.battle;
  CHECK_STR(orderText(b, shape(1, 1, 1, 1, false)), "A1 D1");
  CHECK_STR(orderText(b, shape(1, 1, 0, 1, false)), "A1");
  CHECK_STR(orderText(b, shape(2, 2, 3, 1, false)), "A2 D1 A2 D1 D1");
  CHECK_STR(orderText(b, shape(2, 2, 3, 1, true)), "D1 A2 D1 A2 D1");
  CHECK_STR(orderText(b, shape(3, 1, 1, 4, true)), "D4 A1 A1 A1");
  CHECK_STR(orderText(b, shape(2, 3, 0, 1, true)), "A3 A3");  // nobody answers: "defender first" means nothing
  CHECK_STR(orderText(b, Forecast()), "");
  // The forecast the battle itself makes carries the same order.
  d.combat([](CombatMods& a, CombatMods& def) { a.raiseMultiStrike(2); def.strikeFirst = true; });
  CHECK_STR(orderText(b, d.forecast()), "D1 A2");
}

TEST(combat_order_multi_strike_in_a_fight) {
  Duel d;
  d.w.db.rules.fStrikes = "2";
  d.combat([](CombatMods& a, CombatMods& def) { a.raiseMultiStrike(2); a.damage = -2; def.damage = -2; });
  CHECK(d.begin());
  CHECK(d.attack());
  CHECK_STR(d.story(), "AADAAD");
  CHECK_EQ(d.hp(1), 6);
  CHECK_EQ(d.hp(0), 8);
}

TEST(combat_order_strikes_in_a_row_stop_when_the_foe_falls) {
  Duel d;
  d.combat([](CombatMods& a, CombatMods&) { a.raiseMultiStrike(4); a.damage = 2; });
  CHECK(d.begin());
  CHECK(d.attack());
  CHECK_STR(d.story(), "AA");
  CHECK(!d.w.battle.units[1].alive);
}

TEST(combat_order_strike_first_lets_the_defender_go_first) {
  Duel d;
  d.combat([](CombatMods&, CombatMods& def) { def.strikeFirst = true; });
  CHECK(d.begin());
  CHECK(d.forecast().defenderFirst);
  CHECK(d.attack());
  CHECK_STR(d.story(), "DA");
  CHECK(d.w.events[d.w.events.size() - 2].flags & kCounter);
  CHECK(d.w.battle.units[0].done);
}

TEST(combat_order_strike_first_on_both_or_on_the_attacker_changes_nothing) {
  Duel both;
  both.combat([](CombatMods& a, CombatMods& def) { a.strikeFirst = def.strikeFirst = true; });
  CHECK(both.begin());
  CHECK(!both.forecast().defenderFirst);
  CHECK(both.attack());
  CHECK_STR(both.story(), "AD");
  Duel attacker;
  attacker.combat([](CombatMods& a, CombatMods&) { a.strikeFirst = true; });
  CHECK(attacker.begin());
  CHECK(attacker.attack());
  CHECK_STR(attacker.story(), "AD");
  // A defender that cannot answer is not "first" either.
  Duel ranged("archer", "soldier", 2);
  ranged.combat([](CombatMods&, CombatMods& def) { def.strikeFirst = true; });
  CHECK(ranged.begin());
  CHECK(!ranged.forecast().defenderFirst);
  CHECK(ranged.attack());
  CHECK_STR(ranged.story(), "A");
}

TEST(combat_order_an_attacker_struck_down_first_never_strikes_or_spends_ammunition) {
  Duel d;
  d.w.db.weapons[0].ammo = 3;
  d.combat([](CombatMods&, CombatMods& def) { def.strikeFirst = true; def.damage = 20; });
  CHECK(d.begin());
  CHECK_EQ(d.w.battle.ammo(0, 0), 3);
  CHECK(d.attack());
  CHECK_STR(d.story(), "D");
  CHECK(!d.w.battle.units[0].alive);
  CHECK_EQ(d.w.battle.ammo(1, 0), 2);  // the answer was made
  CHECK(d.w.battle.units[0].ammo.empty() || d.w.battle.units[0].ammo[0] == 3);  // the attack never was
}

TEST(combat_order_ammunition_is_one_per_fighter_whatever_the_order) {
  Duel d;
  d.w.db.weapons[0].ammo = 3;
  d.w.db.rules.fStrikes = "2";
  d.combat([](CombatMods& a, CombatMods& def) {
    def.strikeFirst = true;
    a.raiseMultiStrike(2);
    a.damage = def.damage = -2;
  });
  CHECK(d.begin());
  CHECK(d.attack());
  CHECK_STR(d.story(), "DAADAA");
  CHECK_EQ(d.w.battle.ammo(0, 0), 2);
  CHECK_EQ(d.w.battle.ammo(1, 0), 2);
}

TEST(combat_order_is_fixed_when_the_fight_starts) {
  // Modifiers that only appear once the fight is under way change the numbers of later strikes, never who
  // strikes, how often or whether the defender answers.
  Duel d("archer", "soldier", 2);
  d.w.db.rules.fStrikes = "2";
  d.mods.combat = [](const Battle& b, const CombatSetup& s, CombatMods& a, CombatMods& def) {
    if (b.units[(size_t)s.defender].hp == 10) return;  // nothing before the first strike lands
    a.strikes = 3;
    a.raiseMultiStrike(3);
    def.counterAnyRange = true;
    def.strikeFirst = true;
  };
  CHECK(d.begin());
  CHECK(d.attack());
  CHECK_STR(d.story(), "AA");
}

TEST(combat_counter_any_range_answers_from_out_of_range) {
  Duel d("archer", "soldier", 2);
  CHECK(d.begin());
  CHECK(!d.forecast().counter.can);
  d.combat([](CombatMods&, CombatMods& def) { def.counterAnyRange = true; });
  Forecast f = d.forecast();
  CHECK(f.counter.can);
  CHECK_EQ(f.counter.weapon, 0);
  CHECK_EQ(f.counter.damage, 4);  // 5 - 1
  CHECK(d.attack());
  CHECK_STR(d.story(), "AD");
  CHECK_EQ(d.hp(0), 4);
  // On the attacker it means nothing.
  Duel other("archer", "soldier", 2);
  other.combat([](CombatMods& a, CombatMods&) { a.counterAnyRange = true; });
  CHECK(other.begin());
  CHECK(!other.forecast().counter.can);
}

TEST(combat_counter_any_range_needs_a_weapon_that_may_answer) {
  // A bow never answers ("counter": false), at any range.
  Duel bow("soldier", "archer", 1);
  CombatSetup seen;
  bow.mods.combat = [&](const Battle&, const CombatSetup& s, CombatMods&, CombatMods& def) {
    seen = s;
    def.counterAnyRange = true;
  };
  CHECK(bow.begin());
  CHECK(!bow.forecast().counter.can);
  CHECK_EQ(seen.weaponD, -1);
  CHECK(bow.attack());
  CHECK_STR(bow.story(), "A");
  // A sword cannot hurt a tank at all, so a soldier has nothing to answer a tank with.
  Duel tank("tank", "soldier", 1);
  tank.combat([](CombatMods&, CombatMods& def) { def.counterAnyRange = true; });
  CHECK(tank.begin());
  CHECK(!tank.forecast().counter.can);
  // And a game without counterattacks stays without.
  Duel off("archer", "soldier", 2);
  off.w.db.rules.counterAttacks = false;
  off.combat([](CombatMods&, CombatMods& def) { def.counterAnyRange = true; });
  CHECK(off.begin());
  CHECK(!off.forecast().counter.can);
  CHECK(off.attack());
  CHECK_STR(off.story(), "A");
}

TEST(combat_no_counter_silences_the_foe) {
  Duel d;
  d.combat([](CombatMods& a, CombatMods& def) { a.noCounter = true; def.counterAnyRange = true; def.strikeFirst = true; });
  CHECK(d.begin());
  Forecast f = d.forecast();
  CHECK(!f.counter.can);
  CHECK(!f.defenderFirst);
  CHECK(d.attack());
  CHECK_STR(d.story(), "A");
  CHECK_EQ(d.hp(0), 10);
  // On the defender it is about a counter to its own attacks: here it changes nothing.
  Duel other;
  other.combat([](CombatMods&, CombatMods& def) { def.noCounter = true; });
  CHECK(other.begin());
  CHECK(other.attack());
  CHECK_STR(other.story(), "AD");
}

TEST(combat_counter_with_carried_weapons_is_the_weapon_in_hand) {
  Duel d("archer", "soldier", 2);
  addItems(d.w, true);
  carry(d.w, 0, {items::BowItem});
  carry(d.w, 1, {items::Potion, items::SwordItem});
  d.combat([](CombatMods&, CombatMods& def) { def.counterAnyRange = true; });
  CHECK(d.begin());
  Forecast f = d.forecast();
  CHECK(f.counter.can);
  CHECK_EQ(f.counter.weapon, 0);
  CHECK(d.attack());
  CHECK_STR(d.story(), "AD");
  CHECK_EQ(d.w.battle.units[1].items[1].uses, 1);  // the answer wore the sword (2 uses)
  // Without a weapon in hand there is nothing to answer with.
  Duel bare("archer", "soldier", 2);
  addItems(bare.w, true);
  carry(bare.w, 0, {items::BowItem});
  carry(bare.w, 1, {items::Potion});
  bare.combat([](CombatMods&, CombatMods& def) { def.counterAnyRange = true; });
  CHECK(bare.begin());
  CHECK(!bare.forecast().counter.can);
}
