// Combat pipeline (docs/SRPG.md step F2): one strike, steps 1 to 8. The effects of "strike" / "struck" blocks
// (StrikeMods), survive, drain, strikeAgain and where the events go, all handed in through Battle::testMods (skills
// themselves arrive with step K2). The activation rule and its roll order are in test_srpg_combat_rolls.cpp.
#include "srpg_combat_fixture.h"

using namespace ote;
using namespace combattest;

namespace {

Event activated(int unit, int skill, int foe) {
  Event e;
  e.type = EventType::SkillActivated;
  e.a = unit;
  e.b = skill;
  e.c = foe;
  return e;
}

}  // namespace

TEST(combat_strike_forecast_equals_outcome_when_nothing_is_random) {
  // Blocks without a chance are certain: the forecast shows them and the fight does exactly that.
  Duel d;
  d.strike([](bool counter, StrikeMods& s) {
    if (counter) s.damageTaken = -1;  // the attacker is struck for 1 less
    else { s.damage = 2; s.damagePercent = {150}; s.damageTakenPercent = {50}; }  // (3 + 2) -> 7 -> 3
  });
  CHECK(d.begin());
  Forecast f = d.forecast();
  CHECK_EQ(f.attack.damage, 3);
  CHECK_EQ(f.counter.damage, 2);
  CHECK(d.attack());
  CHECK_EQ(d.hp(1), 10 - f.attack.damage);
  CHECK_EQ(d.hp(0), 10 - f.counter.damage);
}

TEST(combat_strike_hit_and_avoid_of_one_strike) {
  Duel d;
  d.w.db.rules.fHit = "50";
  d.strike([](bool counter, StrikeMods& s) {
    if (counter) s.avoid = 50;  // the attacker cannot be hit
    else s.hit = 50;            // and cannot miss
  });
  CHECK(d.begin());
  CHECK_EQ(d.forecast().attack.hit, 100);
  CHECK_EQ(d.forecast().counter.hit, 0);
  CHECK(d.attack());
  CHECK_STR(d.story(), "Ad");
}

TEST(combat_strike_crit_then_percents) {
  Duel d("soldier", "tank");
  d.w.db.weapons[0].versus.clear();  // let swords hit tanks here
  d.w.db.rules.fDamage = "3";
  d.w.db.rules.fCrit = "60";
  d.w.db.rules.counterAttacks = false;
  d.strike([](bool, StrikeMods& s) { s.crit = 40; s.damage = 1; s.damagePercent = {50}; });
  CHECK(d.begin());
  CHECK_EQ(d.forecast().attack.crit, 100);
  CHECK_EQ(d.forecast().attack.damage, 2);  // (3 + 1) / 2, before any critical
  CHECK(d.attack());
  CHECK_EQ(d.hp(1), 20 - 6);  // (3 + 1) x 3 = 12, then half
  // The struck unit's critAvoid takes the chance away again, and then nothing is rolled for it.
  Duel safe("soldier", "tank");
  safe.w.db.weapons[0].versus.clear();
  safe.w.db.rules.fCrit = "60";
  safe.w.db.rules.counterAttacks = false;
  safe.strike([](bool, StrikeMods& s) { s.critAvoid = 60; });
  CHECK(safe.begin());
  uint64_t rng = safe.w.battle.rng.state;
  CHECK(safe.attack());
  CHECK(safe.w.battle.rng.state == rng);
  CHECK(!(safe.w.last(EventType::Attack)->flags & kCrit));
}

TEST(combat_strike_survive_leaves_one_hp) {
  Duel d;
  d.w.db.rules.counterAttacks = false;
  d.strike([](bool, StrikeMods& s) { s.damage = 20; s.survive = true; });
  CHECK(d.begin());
  CHECK(d.attack());
  CHECK_EQ(d.hp(1), 1);
  const Event* e = d.w.last(EventType::Attack);
  CHECK(e->flags & kSurvived);
  CHECK(!(e->flags & kKill));
  CHECK_EQ(e->d, 9);
  CHECK(d.w.battle.units[1].alive);
  // At 1 HP there is nothing left to survive on.
  d.w.battle.units[0].done = false;
  CHECK(d.attack());
  CHECK(!d.w.battle.units[1].alive);
  e = d.w.last(EventType::Attack);
  CHECK(e->flags & kKill);
  CHECK(!(e->flags & kSurvived));
}

TEST(combat_strike_survive_is_not_flagged_when_the_strike_would_not_defeat) {
  Duel d;
  d.strike([](bool, StrikeMods& s) { s.survive = true; });
  CHECK(d.begin());
  CHECK(d.attack());
  CHECK_EQ(d.hp(1), 7);
  for (const Event& e : d.w.events) CHECK(!(e.flags & kSurvived));
}

TEST(combat_strike_drain_heals_the_striker) {
  Duel d;
  d.w.db.rules.counterAttacks = false;
  d.strike([](bool, StrikeMods& s) { s.drain = 50; });
  CHECK(d.begin());
  d.w.battle.units[0].hp = 5;
  CHECK(d.attack());
  CHECK_STR(d.story(), "Ah");  // the heal comes after the strike
  CHECK_EQ(d.hp(0), 6);        // half of 3, rounded down
  const Event* h = d.w.last(EventType::UnitHealed);
  CHECK(h && h->a == 0 && h->b == 1);
}

TEST(combat_strike_drain_limits) {
  // At least 1 when damage was dealt; never above the maximum; nothing at full HP, on a miss or for 0 damage.
  Duel small;
  small.w.db.rules.counterAttacks = false;
  small.strike([](bool, StrikeMods& s) { s.drain = 1; });
  CHECK(small.begin());
  small.w.battle.units[0].hp = 9;
  CHECK(small.attack());
  CHECK_EQ(small.hp(0), 10);
  Duel big;
  big.w.db.rules.counterAttacks = false;
  big.strike([](bool, StrikeMods& s) { s.drain = 1000; });
  CHECK(big.begin());
  big.w.battle.units[0].hp = 9;
  CHECK(big.attack());
  CHECK_EQ(big.hp(0), 10);
  CHECK_EQ(big.w.last(EventType::UnitHealed)->b, 1);
  big.w.battle.units[0].done = false;
  CHECK(big.attack());  // at full HP now
  CHECK_EQ(big.w.count(EventType::UnitHealed), 1);
  Duel miss;
  miss.w.db.rules.fHit = "0";
  miss.strike([](bool, StrikeMods& s) { s.drain = 50; });
  CHECK(miss.begin());
  miss.w.battle.units[0].hp = 5;
  CHECK(miss.attack());
  CHECK_STR(miss.story(), "ad");
  CHECK_EQ(miss.hp(0), 5);
}

TEST(combat_strike_again_gives_one_extra_strike_only) {
  Duel d;
  d.strike([](bool counter, StrikeMods& s) {
    s.strikeAgain = !counter;  // asked for on every strike of the attacker, also on the extra one
    s.damage = -2;
  });
  CHECK(d.begin());
  CHECK(d.attack());
  CHECK_STR(d.story(), "AAD");
  CHECK_EQ(d.hp(1), 8);
  // With two strikes each: every regular strike gets its extra one, straight after it.
  Duel two;
  two.w.db.rules.fStrikes = "2";
  two.strike([](bool counter, StrikeMods& s) { s.strikeAgain = counter; s.damage = -2; });
  CHECK(two.begin());
  CHECK(two.attack());
  CHECK_STR(two.story(), "ADDADD");
}

TEST(combat_strike_again_also_after_a_miss_but_not_after_a_defeat) {
  Duel miss;
  miss.w.db.rules.fHit = "0";
  miss.w.db.rules.counterAttacks = false;
  miss.strike([](bool, StrikeMods& s) { s.strikeAgain = true; });
  CHECK(miss.begin());
  CHECK(miss.attack());
  CHECK_STR(miss.story(), "aa");
  Duel kill;
  kill.strike([](bool, StrikeMods& s) { s.strikeAgain = true; s.damage = 20; });
  CHECK(kill.begin());
  CHECK(kill.attack());
  CHECK_STR(kill.story(), "A");
}

TEST(combat_strike_activation_events_come_before_the_attack_event) {
  // The striker's activations always show; the struck unit's only when the strike hits.
  for (int hitChance : {100, 0}) {
    Duel d;
    d.w.db.rules.fHit = hitChance ? "100" : "0";
    d.w.db.rules.counterAttacks = false;
    d.mods.strike = [](Battle&, const CombatSetup& s, bool, StrikeMods&, std::vector<Event>& strikeEvents,
                       std::vector<Event>& struckEvents) {
      strikeEvents.push_back(activated(s.attacker, 3, s.defender));
      struckEvents.push_back(activated(s.defender, 4, s.attacker));
    };
    CHECK(d.begin());
    size_t before = d.w.events.size();
    CHECK(d.attack());
    std::vector<Event> ev(d.w.events.begin() + (long)before, d.w.events.end());
    if (hitChance) {
      CHECK_STR(d.story(), "ssA");
      CHECK(ev[0].a == 0 && ev[0].b == 3 && ev[0].c == 1);
      CHECK(ev[1].a == 1 && ev[1].b == 4 && ev[1].c == 0);
    } else {
      CHECK_STR(d.story(), "sa");
      CHECK(ev[0].a == 0 && ev[0].b == 3);
    }
  }
}

TEST(combat_forecast_notes_list_activations_without_touching_the_numbers) {
  Duel d;
  d.mods.notes = [](const Battle&, const CombatSetup& s, std::vector<ForecastNote>& out) {
    ForecastNote n;
    n.unit = s.attacker;
    n.skill = 2;
    n.chance = 35;
    out.push_back(n);
  };
  CHECK(d.begin());
  Forecast f = d.forecast();
  CHECK_EQ((int)f.notes.size(), 1);
  CHECK_EQ(f.notes[0].chance, 35);
  CHECK_EQ(f.attack.damage, 3);
  // A forecast that fails hands back no notes.
  Forecast bad;
  CHECK(!d.w.battle.forecast(0, 1, 1, d.w.battle.units[0].cell, bad));
  CHECK(bad.notes.empty());
}
