// The foe's weapon in formulas (`fw`, docs/SRPG.md "Foe's weapon"): a weapon triangle and weight against hit and
// doubling written with it (tests/fixtures/srpg-triangle), the counterattack's swapped roles, forecast = real fight,
// a foe with no usable weapon, skills' formulas, the computer opponent and a whole battle.
#include "srpg_triangle_fixture.h"

using namespace ote;
using namespace tritest;

TEST(foe_weapon_attack_sees_the_answering_weapon) {
  World w;
  // sw (sword) against ax (axe): sword beats axe. The axe's weight (5) makes it easier to hit.
  const Forecast f = w.forecast("sw", "ax", "iron_sword");
  CHECK_EQ(f.attack.damage, 5 + 5 - 2 + 3);
  CHECK_EQ(f.attack.hit, 70 + 15 + 2 * 5);
  CHECK_EQ(f.attack.strikes, 1);  // 6 - 3 >= 6 - 5 + 5 is false
  // Against the lance the sword loses: -3 damage, -15 hit, the lance weighs 3.
  const Forecast g = w.forecast("sw", "la", "iron_sword");
  CHECK_EQ(g.attack.damage, 5 + 5 - 2 - 3);
  CHECK_EQ(g.attack.hit, 70 - 15 + 2 * 3);
}

TEST(foe_weapon_counter_swaps_the_roles) {
  World w;
  // In the counter `w` is the defender's weapon and `fw` the attacker's.
  const Forecast f = w.forecast("sw", "ax", "iron_sword");
  CHECK(f.counter.can);
  CHECK_EQ(f.counter.damage, 5 + 6 - 2 - 3);       // axe against sword: disadvantage
  CHECK_EQ(f.counter.hit, 70 - 15 + 2 * 3);        // the sword weighs 3
  const Forecast g = w.forecast("sw", "la", "iron_sword");
  CHECK_EQ(g.counter.damage, 5 + 5 - 2 + 3);       // lance against sword: advantage
  CHECK_EQ(g.counter.hit, 70 + 15 + 2 * 3);
}

TEST(foe_weapon_weight_changes_doubling) {
  World w;
  // The scout (speed 12) fights from beside ax / mk. Against the axe (weight 5) 12 - 3 >= 6 - 5 + 5: twice.
  Battle& b = w.b();
  const Forecast f = w.forecast("sc", "ax", "iron_sword", b.grid.at(3, 1));
  CHECK_EQ(f.attack.strikes, 2);
  CHECK_EQ(f.attack.hit, 100);  // 70 + 12 + 15 + 10 = 107, capped
  // The monk holds nothing (weight reads 0): 12 - 3 >= 6 - 0 + 5 is false.
  const Forecast g = w.forecast("sc", "mk", "iron_sword", b.grid.at(2, 0));
  CHECK_EQ(g.attack.strikes, 1);
  CHECK_EQ(g.attack.hit, 70 + 12);
}

TEST(foe_weapon_real_combat_matches_the_forecast) {
  World w;
  const Forecast f = w.forecast("sw", "ax", "iron_sword");
  const int sw = w.id("sw"), ax = w.id("ax");
  const int hpSw = 0, hpAx = 0;
  CHECK(w.b().apply(Action::attack(sw, ax, w.weapon("iron_sword")), w.events));
  int hitsA = 0, hitsD = 0;
  for (const Event& e : w.events) {
    if (e.type != EventType::Attack) continue;
    (e.a == sw ? hitsA : hitsD)++;
    // A strike that hit did the forecast's damage (the fight is the forecast's numbers, rolled).
    if (e.flags & kHit) CHECK_EQ(e.d, e.a == sw ? f.attack.damage : f.counter.damage);
  }
  CHECK_EQ(hitsA, f.attack.strikes);
  CHECK_EQ(hitsD, f.counter.strikes);
  (void)hpSw; (void)hpAx;
}

TEST(foe_weapon_no_usable_weapon_is_the_empty_table) {
  World w;
  // The monk has no weapon at all; the brute carries one it cannot wield (needs rank B, it has E). Both answer
  // nothing, and `fw` is the empty table: tri() sees `fw.none`, `fw.weight` reads 0 and nothing raises.
  const Forecast a = w.forecast("sw", "mk", "iron_sword");
  const Forecast b = w.forecast("sw", "br", "iron_sword");
  CHECK(!a.counter.can && !b.counter.can);
  CHECK_EQ(a.attack.damage, 5 + 5 - 2);
  CHECK_EQ(a.attack.hit, 70);
  CHECK_EQ(b.attack.damage, a.attack.damage);
  CHECK_EQ(b.attack.hit, a.attack.hit);
  std::vector<int> wieldable;
  w.b().weaponsOf(w.unit("br"), wieldable);
  CHECK(wieldable.empty());
}

TEST(foe_weapon_a_formula_can_index_the_empty_table_freely) {
  // A formula that never checks `fw.none` still computes: missing numbers read 0. `fw.none` is nil (false) when the
  // foe has a weapon.
  World w("duel", [](const std::string& dir) {
    swapText(dir + "/game.json", "\"damage\":  \"a.str + w.might - b.def + 3 * tri(w, fw)\"",
             "\"damage\":  \"a.str + w.might - b.def + fw.might * 2 + (fw.none and 100 or 0)\"");
  });
  const Forecast f = w.forecast("sw", "mk", "iron_sword");
  CHECK_EQ(f.attack.damage, 5 + 5 - 2 + 100);
  const Forecast g = w.forecast("sw", "ax", "iron_sword");
  CHECK_EQ(g.attack.damage, 5 + 5 - 2 + 6 * 2);
}

TEST(foe_weapon_skill_conditions_and_chances_see_it) {
  World w("duel", [](const std::string& dir) {
    writeTextFile(dir + "/data/skills.json", R"({ "skills": [
      { "id": "axe_slayer", "name": "Axe Slayer", "blocks": [
        { "when": "combat", "if": [ { "formula": "fw.kind == 2" } ], "do": [ { "damage": 4 } ] },
        { "when": "strike", "chance": "fw.none and 90 or 30", "do": [ { "damagePercent": 200 } ] } ] } ] })");
    swapText(dir + "/data/units.json", "\"wields\": [\"sword\"], \"ranks\": { \"sword\": \"e\" }, \"items\": [\"iron_sword\"] },\n  { \"id\": \"scout\"",
             "\"wields\": [\"sword\"], \"ranks\": { \"sword\": \"e\" }, \"items\": [\"iron_sword\"], \"skills\": [\"axe_slayer\"] },\n  { \"id\": \"scout\"");
  });
  // Against an axe: +4 damage from the combat block, chance 30 for the strike block.
  const Forecast f = w.forecast("sw", "ax", "iron_sword");
  CHECK_EQ(f.attack.damage, 5 + 5 - 2 + 3 + 4);
  CHECK_EQ(f.notes.size(), (size_t)1);
  CHECK_EQ(f.notes[0].chance, 30);
  // Against a lance: no bonus (fw.kind is 3); against the monk the chance is 90 (`fw.none`).
  const Forecast g = w.forecast("sw", "la", "iron_sword");
  CHECK_EQ(g.attack.damage, 5 + 5 - 2 - 3);
  const Forecast h = w.forecast("sw", "mk", "iron_sword");
  CHECK_EQ(h.attack.damage, 5 + 5 - 2);
  CHECK_EQ(h.notes.size(), (size_t)1);
  CHECK_EQ(h.notes[0].chance, 90);
}

TEST(foe_weapon_the_computer_picks_the_favourable_matchup) {
  // rs (sword) stands between a blue axeman (sword beats axe) and a blue lancer (sword loses). It attacks the axeman.
  World w("choice");
  std::vector<Event> ev;
  CHECK(w.b().apply(Action::endTurn(), ev));
  long steps = 0;
  while (w.b().activeSide == 1 && aiTakeStep(w.b(), ev)) CHECK(++steps < 100);
  int target = -1;
  for (const Event& e : ev)
    if (e.type == EventType::Attack && e.a == w.id("rs")) { target = e.b; break; }
  CHECK_EQ(target, w.id("bx"));
}

namespace {

// A whole battle on the fixture played by the AI on both sides; the trace is every event and the final state.
std::string playTrace(uint32_t seed, bool& stalled) {
  savetest::Rig rig;
  CHECK(rig.load(ote_test::sourceDir() + "/tests/fixtures/srpg-triangle", "duel", seed));
  Battle& b = rig.battle;
  std::vector<Event> ev;
  b.start(ev);
  std::string trace;
  while (!b.over && b.round <= 60) {
    long steps = 0;
    while (aiTakeStep(b, ev))
      if (++steps > 300) { stalled = true; return trace; }
  }
  stalled = stalled || !b.over;
  for (const Event& e : ev)
    trace += std::to_string((int)e.type) + ":" + std::to_string(e.a) + "," + std::to_string(e.b) + "," +
             std::to_string(e.c) + "," + std::to_string(e.d) + ";";
  return trace + savetest::stateText(b);
}

}  // namespace

TEST(foe_weapon_sim_on_the_fixture_ends_and_repeats) {
  for (uint32_t seed = 1; seed <= 10; ++seed) {
    bool s1 = false, s2 = false;
    const std::string a = playTrace(seed, s1), b = playTrace(seed, s2);
    CHECK(!s1 && !s2);
    CHECK(a == b);
  }
}
