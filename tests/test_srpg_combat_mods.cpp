// Combat pipeline (docs/SRPG.md step F2): each field of CombatMods alone, in the forecast and in the fight, handed
// in through Battle::testMods. The order of strikes and the counter rules are in test_srpg_combat_order.cpp.
#include "srpg_combat_fixture.h"

using namespace ote;
using namespace combattest;

namespace {
enum { Hp, Atk, Def, Mov };  // stat indices of the miniature game
}

TEST(combat_mods_nothing_injected_is_the_plain_fight) {
  Duel d;
  CHECK(d.begin());
  Forecast f = d.forecast();
  CHECK_EQ(f.attack.damage, 3);
  CHECK_EQ(f.attack.hit, 100);
  CHECK_EQ(f.attack.crit, 0);
  CHECK_EQ(f.attack.strikes, 1);
  CHECK_EQ(f.attack.multi, 1);
  CHECK(f.counter.can);
  CHECK_EQ(f.counter.multi, 1);
  CHECK(!f.defenderFirst);
  CHECK(f.notes.empty());
  uint64_t rng = d.w.battle.rng.state;
  CHECK(d.attack());
  CHECK_STR(d.story(), "AD");
  CHECK_EQ(d.hp(0), 7);
  CHECK_EQ(d.hp(1), 7);
  CHECK(d.w.battle.rng.state == rng);  // a certain hit and no critical chance: nothing is rolled
}

TEST(combat_mods_stats_enter_the_formulas_of_both_sides) {
  Duel d;
  d.combat([](CombatMods& a, CombatMods& def) {
    a.addStat(Atk, 2);    // the attacker hits harder: 7 - 2 = 5
    def.addStat(Def, 1);  // ... into more defence: 4
    def.addStat(Atk, -1);  // and the defender's answer is weaker: 4 - 2 = 2
  });
  CHECK(d.begin());
  Forecast f = d.forecast();
  CHECK_EQ(f.attack.damage, 4);
  CHECK_EQ(f.counter.damage, 2);
  CHECK(d.attack());
  CHECK_EQ(d.hp(1), 6);
  CHECK_EQ(d.hp(0), 8);
  // A stat with a bonus does not go below 0 inside the formulas.
  d.combat([](CombatMods&, CombatMods& def) { def.addStat(Def, -50); });
  CHECK_EQ(d.forecast().attack.damage, 5);
  // Standing stats are untouched: the bonus lives only inside the combat.
  CHECK_EQ(d.w.battle.stat(d.w.battle.units[0], Atk), 5);
}

TEST(combat_mods_hit_and_avoid) {
  Duel d;
  d.w.db.rules.fHit = "50";
  CHECK(d.begin());
  CHECK_EQ(d.forecast().attack.hit, 50);
  d.combat([](CombatMods& a, CombatMods&) { a.hit = 15; });
  CHECK_EQ(d.forecast().attack.hit, 65);
  CHECK_EQ(d.forecast().counter.hit, 50);
  d.combat([](CombatMods&, CombatMods& def) { def.avoid = 20; });
  CHECK_EQ(d.forecast().attack.hit, 30);
  d.combat([](CombatMods& a, CombatMods&) { a.avoid = 20; });  // the attacker's avoid is against the counter
  CHECK_EQ(d.forecast().attack.hit, 50);
  CHECK_EQ(d.forecast().counter.hit, 30);
  d.combat([](CombatMods& a, CombatMods& def) { a.hit = 500; def.avoid = -500; });
  CHECK_EQ(d.forecast().attack.hit, 100);
  d.combat([](CombatMods&, CombatMods& def) { def.avoid = 500; });
  CHECK_EQ(d.forecast().attack.hit, 0);
  CHECK(d.attack());
  CHECK_EQ(d.hp(1), 10);  // 0 % never hits
}

TEST(combat_mods_limits_apply_to_formula_plus_modifiers) {
  // The formula's own value is not cut to 0..100 first: 130 with 20 avoid against it is still a certain hit.
  Duel d;
  d.w.db.rules.fHit = "130";
  d.combat([](CombatMods&, CombatMods& def) { def.avoid = 20; });
  CHECK(d.begin());
  CHECK_EQ(d.forecast().attack.hit, 100);
  // Damage the same way: an archer does 4 - 5 = -1 to a tank, so +3 damage makes 2, not 3.
  Duel weak("archer", "tank", 2);
  weak.combat([](CombatMods& a, CombatMods&) { a.damage = 3; });
  CHECK(weak.begin());
  CHECK_EQ(weak.forecast().attack.damage, 2);
}

TEST(combat_mods_crit_and_crit_avoid) {
  Duel d("soldier", "tank");
  d.w.db.weapons[0].versus.clear();  // let swords hit tanks here
  d.w.db.rules.fDamage = "2";
  d.w.db.rules.fCrit = "20";
  CHECK(d.begin());
  CHECK_EQ(d.forecast().attack.crit, 20);
  d.combat([](CombatMods& a, CombatMods&) { a.crit = 30; });
  CHECK_EQ(d.forecast().attack.crit, 50);
  d.combat([](CombatMods&, CombatMods& def) { def.critAvoid = 15; });
  CHECK_EQ(d.forecast().attack.crit, 5);
  d.combat([](CombatMods& a, CombatMods&) { a.crit = 80; });
  CHECK_EQ(d.forecast().attack.crit, 100);
  d.w.db.rules.counterAttacks = false;
  CHECK(d.attack());
  CHECK_EQ(d.hp(1), 14);  // 2 x 3, a certain critical
  CHECK(d.w.last(EventType::Attack)->flags & kCrit);
}

TEST(combat_mods_crit_avoided_to_zero_rolls_nothing) {
  Duel d;
  d.w.db.rules.fCrit = "40";
  d.combat([](CombatMods& a, CombatMods& def) { a.critAvoid = 40; def.critAvoid = 90; });
  CHECK(d.begin());
  CHECK_EQ(d.forecast().attack.crit, 0);
  CHECK_EQ(d.forecast().counter.crit, 0);
  uint64_t rng = d.w.battle.rng.state;
  CHECK(d.attack());
  CHECK(d.w.battle.rng.state == rng);
  CHECK_EQ(d.hp(1), 7);
}

TEST(combat_mods_damage_and_damage_taken) {
  Duel d;
  d.combat([](CombatMods& a, CombatMods&) { a.damage = 3; });
  CHECK(d.begin());
  CHECK_EQ(d.forecast().attack.damage, 6);
  CHECK_EQ(d.forecast().counter.damage, 3);
  d.combat([](CombatMods&, CombatMods& def) { def.damageTaken = -2; });
  CHECK_EQ(d.forecast().attack.damage, 1);
  d.combat([](CombatMods& a, CombatMods& def) { a.damage = 1; def.damageTaken = 2; a.damageTaken = -1; });
  CHECK_EQ(d.forecast().attack.damage, 6);
  CHECK_EQ(d.forecast().counter.damage, 2);
  CHECK(d.attack());
  CHECK_EQ(d.hp(1), 4);
  CHECK_EQ(d.hp(0), 8);
  d.combat([](CombatMods&, CombatMods& def) { def.damageTaken = -50; });  // never below 0
  CHECK_EQ(d.forecast().attack.damage, 0);
}

TEST(combat_mods_percents_multiply_in_order_with_whole_numbers) {
  Duel d;
  CHECK(d.begin());
  d.combat([](CombatMods& a, CombatMods&) { a.damagePercent = {150, 50}; });  // 3 -> 4 -> 2
  CHECK_EQ(d.forecast().attack.damage, 2);
  d.combat([](CombatMods& a, CombatMods&) { a.damagePercent = {50, 150}; });  // 3 -> 1 -> 1
  CHECK_EQ(d.forecast().attack.damage, 1);
  // The striker's percents first, then the struck unit's.
  d.combat([](CombatMods& a, CombatMods& def) { a.damagePercent = {50}; def.damageTakenPercent = {300}; });
  CHECK_EQ(d.forecast().attack.damage, 3);  // 3 -> 1 -> 3 (the other way round would be 9 -> 4)
  // The flat numbers are added before any percent.
  d.combat([](CombatMods& a, CombatMods&) { a.damage = 1; a.damagePercent = {200}; });
  CHECK_EQ(d.forecast().attack.damage, 8);
  d.combat([](CombatMods&, CombatMods& def) { def.damageTakenPercent = {0}; });  // 0 = none
  CHECK_EQ(d.forecast().attack.damage, 0);
  CHECK_EQ(d.forecast().counter.damage, 3);
  CHECK(d.attack());
  CHECK_EQ(d.hp(1), 10);
  CHECK_STR(d.story(), "AD");  // it hit, for nothing
}

TEST(combat_mods_apply_percents_stays_in_range) {
  CHECK_EQ(applyPercents(10, {}), 10);
  CHECK_EQ(applyPercents(7, {50}), 3);
  CHECK_EQ(applyPercents(7, {-20}), 0);  // a percent below 0 counts as 0
  CHECK_EQ(applyPercents(-4, {200}), 0);
  CHECK_EQ(applyPercents(900000, {1000, 1000, 1000}), kMaxStrikeDamage);
}

TEST(combat_mods_strikes_are_added_and_kept_within_one_to_eight) {
  Duel d;
  d.w.db.rules.fStrikes = "2";
  CHECK(d.begin());
  d.combat([](CombatMods& a, CombatMods& def) { a.strikes = 1; def.strikes = -1; });
  CHECK_EQ(d.forecast().attack.strikes, 3);
  CHECK_EQ(d.forecast().counter.strikes, 1);
  d.combat([](CombatMods& a, CombatMods& def) { a.strikes = -7; def.strikes = 7; });
  CHECK_EQ(d.forecast().attack.strikes, 1);
  CHECK_EQ(d.forecast().counter.strikes, 8);
  d.combat([](CombatMods& a, CombatMods&) { a.strikes = 1; a.damageTaken = -3; });
  CHECK(d.attack());
  CHECK_STR(d.story(), "ADADA");
  CHECK_EQ(d.hp(1), 1);
  CHECK_EQ(d.hp(0), 10);
}

TEST(combat_mods_multi_strike_shows_in_the_forecast_and_keeps_the_highest) {
  CombatMods m;
  m.raiseMultiStrike(3);
  m.raiseMultiStrike(2);
  CHECK_EQ(m.multiStrike, 3);
  Duel d;
  CHECK(d.begin());
  d.combat([](CombatMods& a, CombatMods& def) { a.raiseMultiStrike(2); def.multiStrike = 9; });
  Forecast f = d.forecast();
  CHECK_EQ(f.attack.multi, 2);
  CHECK_EQ(f.counter.multi, 4);  // never more than four in a row
  CHECK_EQ(f.attack.strikes, 1);  // the number of strikes itself is not changed
}

TEST(combat_mods_sources_are_asked_with_the_fight_they_are_for) {
  Duel d("archer", "soldier", 3);
  CombatSetup seen;
  d.mods.combat = [&](const Battle&, const CombatSetup& s, CombatMods&, CombatMods&) { seen = s; };
  CHECK(d.begin());
  // A forecast from the square the archer would shoot from, not the one it stands on.
  Forecast f;
  CHECK(d.w.battle.forecast(0, 1, 1, d.w.at(1, 0), f));
  CHECK_EQ(seen.attacker, 0);
  CHECK_EQ(seen.defender, 1);
  CHECK_EQ(seen.weaponA, 1);
  CHECK_EQ(seen.weaponD, 0);  // the sword is the soldier's weapon in use although it cannot answer from here
  CHECK_EQ(seen.from, d.w.at(1, 0));
  CHECK(!f.counter.can);
}

TEST(combat_mods_are_judged_again_before_every_strike) {
  Duel d("soldier", "tank");
  d.w.db.weapons[0].versus.clear();
  d.w.db.rules.fStrikes = "3";
  d.w.db.rules.counterAttacks = false;
  // +2 damage once the foe is wounded (a "combat" block with an HP condition would do this).
  d.mods.combat = [](const Battle& b, const CombatSetup& s, CombatMods& a, CombatMods&) {
    if (b.units[(size_t)s.defender].hp < 20) a.damage = 2;
  };
  d.w.db.rules.fDamage = "1";
  CHECK(d.begin());
  CHECK_EQ(d.forecast().attack.damage, 1);  // judged on the state before the fight
  d.w.battle.units[1].hp = 19;
  CHECK_EQ(d.forecast().attack.damage, 3);
  d.w.battle.units[1].hp = 20;
  CHECK(d.attack());
  CHECK_EQ(d.hp(1), 20 - 1 - 3 - 3);
}
