// The hit roll option and formulas that see sides (docs/SRPG.md "The hit roll", "Formulas: sides"): `rules.hitRoll` "average2"
// against "single" with the battle's own dice, and `side` / `team` / `human` in a unit table.
#include "srpg_combat_fixture.h"
#include "srpg_ranks_fixture.h"

#include <cstdio>

using namespace ote;
using namespace combattest;

namespace {

// One archer shot at a soldier two squares away (the soldier cannot answer): exactly one Attack event, one hit roll.
struct Shot {
  Duel d{"archer", "soldier", 2};
  bool hit = false;
  Rng before;  // the battle's dice just before the shot
  Shot(int percent, bool average, uint32_t seed) {
    d.w.db.rules.fHit = std::to_string(percent);
    d.w.db.rules.hitRollAverage = average;
    CHECK(d.begin(seed));
    before = d.w.battle.rng;
    CHECK(d.attack());
    for (const Event& e : d.w.events)
      if (e.type == EventType::Attack) hit = (e.flags & kHit) != 0;
  }
};

}  // namespace

TEST(hit_roll_single_is_todays_one_number) {
  for (uint32_t seed = 1; seed <= 40; ++seed) {
    Shot s(60, false, seed);
    Rng r = s.before;
    CHECK_EQ(s.hit ? 1 : 0, r.chance(60) ? 1 : 0);
    CHECK(s.d.w.battle.rng.state == r.state);  // exactly one number spent (no crit in this game)
  }
}

TEST(hit_roll_average2_uses_two_numbers_and_their_average) {
  for (uint32_t seed = 1; seed <= 60; ++seed) {
    Shot s(70, true, seed);
    Rng r = s.before;
    const int a = r.range(100), b = r.range(100);
    CHECK_EQ(s.hit ? 1 : 0, (a + b) / 2 < 70 ? 1 : 0);  // the average (rounded down) against the chance
    CHECK(s.d.w.battle.rng.state == r.state);            // exactly two numbers spent
  }
}

TEST(hit_roll_average2_exact_small_case_enumeration) {
  // Every pair of numbers 0..99: how many have floor((a + b) / 2) below the chance. A hit chance of 50 is 50.5% (5050 of
  // 10000), of 25 it is 12.75%, of 75 it is 87.75%: the classic "true hit" pulls the odds away from 50.
  const int chances[] = {0, 1, 25, 50, 75, 99};
  const int expected[] = {0, 3, 1275, 5050, 8775, 10000 - 1};
  for (int i = 0; i < 6; ++i) {
    int count = 0;
    for (int a = 0; a < 100; ++a)
      for (int b = 0; b < 100; ++b) count += (a + b) / 2 < chances[i];
    if (chances[i] == 99) CHECK(count > 9900);  // everything but the (99, 99) corner and a few high pairs
    else CHECK_EQ(count, expected[i]);
  }
}

TEST(hit_roll_average2_distribution_with_fixed_seeds) {
  // The real battle over many seeds: 75 shown, about 87.75% real; 25 shown, about 12.75% real; single stays at its nominal.
  int hits75 = 0, hits25 = 0, single75 = 0;
  const int runs = 1500;
  for (int seed = 1; seed <= runs; ++seed) {
    hits75 += Shot(75, true, (uint32_t)seed).hit;
    hits25 += Shot(25, true, (uint32_t)seed).hit;
    single75 += Shot(75, false, (uint32_t)seed).hit;
  }
  std::printf("  average2 at 75: %d/%d, at 25: %d/%d, single at 75: %d/%d\n", hits75, runs, hits25, runs, single75, runs);
  CHECK(hits75 > runs * 8450 / 10000 && hits75 < runs * 9100 / 10000);
  CHECK(hits25 > runs * 1000 / 10000 && hits25 < runs * 1550 / 10000);
  CHECK(single75 > runs * 7200 / 10000 && single75 < runs * 7800 / 10000);
}

TEST(hit_roll_forecast_still_shows_the_nominal_chance) {
  Duel d("archer", "soldier", 2);
  d.w.db.rules.fHit = "75";
  d.w.db.rules.hitRollAverage = true;
  CHECK(d.begin());
  CHECK_EQ(d.forecast().attack.hit, 75);
}

TEST(hit_roll_a_sure_hit_rolls_nothing_and_staves_stay_single) {
  Shot s(100, true, 5);
  CHECK(s.hit);
  CHECK(s.d.w.battle.rng.state == s.before.state);  // a chance of 100 never rolled, as before
}

TEST(hit_roll_loader_reads_the_key_and_refuses_other_words) {
  const char* from = "\"counterAttacks\": true,";
  rankstest::Copy c("fid_hitroll", "srpg-conditions");
  CHECK_STR(c.gameError(), "");
  Database db;
  std::string err;
  CHECK(loadGame(c.dir, db, err));
  CHECK(!db.rules.hitRollAverage);  // a game without the key rolls as before
  c.swap("game.json", from, "\"counterAttacks\": true, \"hitRoll\": \"average2\",");
  CHECK(loadGame(c.dir, db, err));
  CHECK(db.rules.hitRollAverage);
  c.swap("game.json", "\"hitRoll\": \"average2\"", "\"hitRoll\": \"single\"");
  CHECK(loadGame(c.dir, db, err) && !db.rules.hitRollAverage);
  c.swap("game.json", "\"hitRoll\": \"single\"", "\"hitRoll\": \"triple\"");
  CHECK(rankstest::saysIn(c.gameError(), "\"hitRoll\" must be \"single\" or \"average2\""));
  c.swap("game.json", "\"hitRoll\": \"triple\"", "\"hitRoll\": 2");
  CHECK(!c.gameError().empty());
}

// ---- formulas that see sides ----

TEST(formulas_see_side_team_and_human) {
  // blue is the human side, red the computer's. The damage formula packs the three fields into one number.
  Duel d("soldier", "soldier", 1);
  d.w.db.rules.fDamage = "(a.human and 100 or 0) + a.team * 10 + #a.side + (b.human and 1000 or 0)";
  CHECK(d.begin());
  const Forecast f = d.forecast();
  const Unit& atk = d.w.battle.units[0];
  const Unit& def = d.w.battle.units[1];
  const SideState& sa = d.w.battle.sides[(size_t)atk.side];
  const SideState& sd = d.w.battle.sides[(size_t)def.side];
  CHECK(!sa.ai && sd.ai);
  CHECK_EQ(f.attack.damage, 100 + sa.team * 10 + (int)sa.id.size());  // the foe is red: not human
  // The counter has the roles swapped: a = red (not human), b = blue (human).
  CHECK_EQ(f.counter.damage, 0 + sd.team * 10 + (int)sd.id.size() + 1000);
}

TEST(formulas_side_id_compares_as_text) {
  Duel d("soldier", "soldier", 1);
  d.w.db.rules.fDamage = "(a.side == 'blue') and 7 or 3";
  CHECK(d.begin());
  const Forecast f = d.forecast();
  CHECK_EQ(f.attack.damage, 7);
  CHECK_EQ(f.counter.damage, 3);
}
