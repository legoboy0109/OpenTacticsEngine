// Combat pipeline (docs/SRPG.md step F2): activations and the random numbers. A chance of 0 or 100 rolls nothing,
// activations are rolled in list order before the strike's hit roll, and so their order can decide a fight.
// The "skills" here are stand-ins handed in through Battle::testMods (real ones arrive with step K2).
#include "srpg_combat_fixture.h"

using namespace ote;
using namespace combattest;

TEST(combat_activation_chance_of_0_or_100_rolls_nothing) {
  Rng rng;
  rng.seed(5);
  uint64_t start = rng.state;
  CHECK(!rollActivation(rng, 0));
  CHECK(!rollActivation(rng, -30));
  CHECK(rollActivation(rng, 100));
  CHECK(rollActivation(rng, 250));
  CHECK(rng.state == start);
  rollActivation(rng, 50);
  CHECK(rng.state != start);
  // 1..99 is an honest percent.
  int fired = 0;
  for (int i = 0; i < 2000; ++i) fired += rollActivation(rng, 30);
  CHECK(fired > 500 && fired < 700);
}

TEST(combat_activation_roll_order_decides_the_outcome) {
  // Two activations at 50 %: "heavy" (+5 damage) and "feint" (the strike does nothing). They are rolled in list
  // order, so on a seed where the first roll succeeds and the second fails, which one stands first decides.
  auto fight = [](uint32_t seed, bool heavyFirst, bool& roll1, bool& roll2) {
    Duel d;
    d.w.db.rules.counterAttacks = false;
    bool r1 = false, r2 = false;
    d.mods.strike = [&](Battle& b, const CombatSetup&, bool, StrikeMods& s, std::vector<Event>&, std::vector<Event>&) {
      r1 = rollActivation(b.rng, 50);
      r2 = rollActivation(b.rng, 50);
      bool heavy = heavyFirst ? r1 : r2, feint = heavyFirst ? r2 : r1;
      if (heavy) s.damage += 5;
      if (feint) s.damagePercent.push_back(0);
    };
    CHECK(d.begin(seed));
    CHECK(d.attack());
    roll1 = r1;
    roll2 = r2;
    return 10 - d.hp(1);
  };
  int found = 0;
  for (uint32_t seed = 1; seed <= 40; ++seed) {
    bool r1, r2, s1, s2;
    int heavyFirst = fight(seed, true, r1, r2);
    int feintFirst = fight(seed, false, s1, s2);
    CHECK(r1 == s1 && r2 == s2);  // the same random numbers either way
    if (!(r1 && !r2)) continue;
    ++found;
    CHECK_EQ(heavyFirst, 8);  // heavy fired, feint did not
    CHECK_EQ(feintFirst, 0);  // feint took the lucky roll
  }
  CHECK(found > 0);
}

TEST(combat_activations_roll_before_the_hit_roll) {
  // The same seed with and without a rolled activation: the hit roll moves one random number on.
  auto rolls = [](bool activation) {
    Duel d;
    d.w.db.rules.fHit = "50";
    d.w.db.rules.counterAttacks = false;
    std::string s;
    d.mods.strike = [&, activation](Battle& b, const CombatSetup&, bool, StrikeMods&, std::vector<Event>&,
                                    std::vector<Event>&) {
      if (activation) s += rollActivation(b.rng, 50) ? 'y' : 'n';
    };
    CHECK(d.begin(9));
    for (int i = 0; i < 12; ++i) {
      d.w.battle.units[0].done = false;
      d.w.battle.units[1].hp = 10;
      CHECK(d.attack());
    }
    return s + "|" + d.story();
  };
  // Twelve plain attacks consume twelve numbers: h1..h12. With an activation each attack consumes two, the
  // activation's first: (a1 h1') (a2 h2') ... so the plain hit sequence shows up interleaved.
  std::string plain = rolls(false), with = rolls(true);
  std::string hits = plain.substr(1), acts = with.substr(0, 12), hits2 = with.substr(13);
  for (int i = 0; i < 6; ++i) {
    CHECK((acts[(size_t)i] == 'y') == (hits[(size_t)(2 * i)] == 'A'));       // the activation took the odd numbers
    CHECK((hits2[(size_t)i] == 'A') == (hits[(size_t)(2 * i + 1)] == 'A'));  // the hit roll the even ones
  }
}
