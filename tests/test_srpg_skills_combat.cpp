// Step K2 of docs/SRPG.md: "combat" blocks and auras with real skills. A duel: unit 0 ("ua", blue) attacks unit 1
// ("ub", red) for 4 damage with 100 % hit and 0 % crit, and is answered for 4. (tests/srpg_skills_duel.h)
#include "srpg_skills_duel.h"

using namespace ote;
using namespace duel;

namespace {

std::string D(const std::string& effects, const std::string& cond = "") { return block("combat", effects, cond); }

}  // namespace

TEST(skills_combat_numbers_reach_the_forecast) {
  Options o;
  o.aSkills = "\"att\", \"jinx\"";
  o.dSkills = "\"def\"";
  Duel d(skill("att", D("{ \"damage\": 3 }, { \"crit\": 20 }, { \"stats\": { \"atk\": 1 } }")) + "," +
         skill("jinx", D("{ \"avoid\": -20, \"to\": \"foe\" }, { \"hit\": -10, \"to\": \"foe\" }")) + "," +
         skill("def", D("{ \"avoid\": 30 }, { \"critAvoid\": 5 }, { \"damageTaken\": -1 }")), o);
  const Forecast f = d.forecast();
  // 6 + 1 atk - 2 def = 5, +3 damage, -1 damage taken; hit 100 - (30 - 20) = 90; crit 20 - 5.
  CHECK_EQ(f.attack.damage, 7);
  CHECK_EQ(f.attack.hit, 90);
  CHECK_EQ(f.attack.crit, 15);
  // The defender: 6 - 2 = 4, its hit lowered by the attacker's "to foe" 10, nothing else.
  CHECK_EQ(f.counter.damage, 4);
  CHECK_EQ(f.counter.hit, 90);
  CHECK_EQ(f.counter.crit, 0);
  CHECK(f.notes.empty());  // passive blocks make no notes
}

TEST(skills_combat_conditions_use_who_started_the_fight) {
  Options o;
  o.aSkills = "\"opening\"";
  o.dSkills = "\"hold\"";
  Duel d(skill("opening", D("{ \"damage\": 3 }", "{ \"initiating\": true }")) + "," +
         skill("hold", D("{ \"damageTaken\": -2 }", "{ \"initiating\": false }")), o);
  Forecast f = d.forecast(0, 1);  // unit 0 attacks: opening applies, hold applies for the defender
  CHECK_EQ(f.attack.damage, 4 + 3 - 2);
  CHECK_EQ(f.counter.damage, 4);
  f = d.forecast(1, 0);  // unit 1 attacks: it is not "initiating == false"; unit 0 is the defender
  CHECK_EQ(f.attack.damage, 4);
  CHECK_EQ(f.counter.damage, 4);  // opening needs initiating: unit 0 only defends
}

TEST(skills_combat_condition_on_the_foe_and_on_distance) {
  Options o;
  o.aSkills = "\"close\", \"far\"";
  o.dSkills = "\"x\"";
  Duel d(skill("close", D("{ \"damage\": 2 }", "{ \"distance\": { \"op\": \"=\", \"value\": 1 } }")) + "," +
         skill("far", D("{ \"damage\": 50 }", "{ \"distance\": { \"op\": \">=\", \"value\": 2 } }")) + "," +
         skill("x", D("{ \"damage\": 1 }", "{ \"hp\": { \"op\": \"<\", \"percent\": 50 }, \"who\": \"foe\" }")), o);
  CHECK_EQ(d.forecast().attack.damage, 6);  // close applies, far does not
  CHECK_EQ(d.forecast().counter.damage, 4);  // the foe (unit 0) is at full health
  d.u(0).hp = 10;
  CHECK_EQ(d.forecast().counter.damage, 5);
}

TEST(skills_combat_percents_multiply_in_the_order_of_the_skill_list) {
  auto damage = [](const char* list) {
    Options o;
    o.aSkills = list;
    Duel d(skill("p150", D("{ \"damagePercent\": 150 }")) + "," + skill("p90", D("{ \"damagePercent\": 90 }")), o);
    return d.forecast().attack.damage;
  };
  CHECK_EQ(damage("\"p150\", \"p90\""), 5);  // 4 * 150 / 100 = 6, 6 * 90 / 100 = 5
  CHECK_EQ(damage("\"p90\", \"p150\""), 4);  // 4 * 90 / 100 = 3, 3 * 150 / 100 = 4
}

TEST(skills_combat_strike_counts_and_the_order_of_strikes) {
  Options o;
  o.aSkills = "\"more\"";
  Duel d(skill("more", D("{ \"strikes\": 1 }")), o);
  CHECK_EQ(d.forecast().attack.strikes, 2);
  CHECK(d.attack());
  CHECK_STR(d.story(), "ADA");  // two rounds: the defender has one strike only

  Options m;
  m.aSkills = "\"multi\"";
  Duel e(skill("multi", D("{ \"multiStrike\": 2 }")), m);
  CHECK_EQ(e.forecast().attack.multi, 2);
  CHECK(e.attack());
  CHECK_STR(e.story(), "AAD");
  CHECK_EQ(e.hp(1), 30 - 8);
  CHECK_EQ(e.hp(0), 30 - 4);
}

TEST(skills_combat_strike_first_no_counter_and_any_range) {
  Options o;
  o.dSkills = "\"first\"";
  Duel d(skill("first", D("{ \"strikeFirst\": true }")), o);
  CHECK(d.forecast().defenderFirst);
  CHECK(d.attack());
  CHECK_STR(d.story(), "DA");

  Options n;
  n.aSkills = "\"quiet\"";
  Duel e(skill("quiet", D("{ \"noCounter\": true }")), n);
  CHECK(!e.forecast().counter.can);
  CHECK(e.attack());
  CHECK_STR(e.story(), "A");

  // A bow hits from two squares away, where a sword cannot answer: only "counterAnyRange" makes it answer.
  Options r;
  r.aWeapon = "plain_bow";
  r.gap = 2;
  Duel plain(skill("any", D("{ \"counterAnyRange\": true }")), r);
  CHECK(!plain.forecast().counter.can);
  r.dSkills = "\"any\"";
  Duel any(skill("any", D("{ \"counterAnyRange\": true }")), r);
  CHECK(any.forecast().counter.can);
  CHECK(any.attack());
  CHECK_STR(any.story(), "AD");
}

TEST(skills_combat_stats_and_to_foe_are_added_inside_the_formulas) {
  Options o;
  o.aSkills = "\"edge\"";
  Duel d(skill("edge", D("{ \"stats\": { \"atk\": 2 } }, { \"stats\": { \"def\": -1 }, \"to\": \"foe\" }")), o);
  const Forecast f = d.forecast();
  CHECK_EQ(f.attack.damage, 4 + 2 + 1);
  CHECK_EQ(f.counter.damage, 4);  // the lowered def is the foe's: it does not change what the foe deals
}

TEST(skills_combat_blocks_are_judged_again_before_every_strike) {
  // "wounded": +5 damage while at half HP or less. Unit 0 starts at 16 (not wounded), is hit by the counter to 12
  // and so strikes its second strike (from "more") with +5.
  Options o;
  o.aSkills = "\"more\", \"wounded\"";
  Duel d(skill("more", D("{ \"strikes\": 1 }")) + "," +
         skill("wounded", D("{ \"damage\": 5 }", "{ \"hp\": { \"op\": \"<=\", \"percent\": 50 } }")), o);
  d.u(0).hp = 16;
  CHECK_EQ(d.forecast().attack.damage, 4);
  CHECK(d.attack());
  CHECK_EQ(d.hp(1), 30 - 4 - 9);
}

TEST(skills_combat_weapon_skills_follow_the_weapon_in_use) {
  Options o;
  o.dWeapon = "hexed_sword";
  Duel d(skill("weapon_skill", D("{ \"damage\": 3 }")), o);
  const Forecast f = d.forecast();
  CHECK_EQ(f.attack.damage, 4);
  CHECK_EQ(f.counter.damage, 7);  // the defender fights with the hexed sword
}

TEST(skills_aura_reaches_fighters_near_the_owner_but_never_the_owner) {
  // "ua" units carry the aura (allies within 2: +2 damage). Unit 0 gets it only from a second one near it.
  const std::string aura = skill("rally", "{ \"when\": \"combat\", \"aura\": { \"to\": \"allies\", \"within\": 2 }, "
                                          "\"do\": [ { \"damage\": 2 } ] }");
  Options alone;
  alone.aSkills = "\"rally\"";
  Duel a(aura, alone);
  CHECK_EQ(a.forecast().attack.damage, 4);  // its own aura does not reach itself
  Options near_ = alone;
  near_.extraUnits = ", { \"type\": \"ua\", \"side\": \"blue\", \"x\": 0, \"y\": 2 }";  // unit 2, one step away
  Duel b(aura, near_);
  CHECK_EQ(b.forecast().attack.damage, 6);
  b.u(2).hp = 1;  // the owner need not be healthy, only on the board
  CHECK_EQ(b.forecast().attack.damage, 6);
  Options far_ = alone;
  far_.extraUnits = ", { \"type\": \"ua\", \"side\": \"blue\", \"x\": 6, \"y\": 2 }";
  Duel c(aura, far_);
  CHECK_EQ(c.forecast().attack.damage, 4);  // too far
}

TEST(skills_aura_of_enemies_and_the_owners_conditions) {
  // Red unit 2 ("uc") with "chill": enemies within 2 deal 3 less. Judged for the owner: only at half HP or less.
  const std::string chill =
      skill("chill", "{ \"when\": \"combat\", \"aura\": { \"to\": \"enemies\", \"within\": 2 }, "
                     "\"if\": [ { \"hp\": { \"op\": \"<=\", \"percent\": 50 } } ], \"do\": [ { \"damage\": -3 } ] }");
  Options o;
  o.extraUnits = ", { \"type\": \"uc\", \"side\": \"red\", \"x\": 2, \"y\": 2 }";
  Duel d(chill, o);
  d.u(2).hp = 30;
  // uc has no skills: give it one through the story-free way, the learned list.
  d.u(2).learned.push_back(d.skill("chill"));
  CHECK_EQ(d.forecast().attack.damage, 4);  // the owner is healthy
  d.u(2).hp = 10;
  CHECK_EQ(d.forecast().attack.damage, 4);  // unit 0 at (0,1) is 3 steps from (2,2): out of reach
  d.u(2).cell = d.w.at(1, 2);
  d.b().occupant[(size_t)d.w.at(2, 2)] = -1;
  d.b().occupant[(size_t)d.w.at(1, 2)] = 2;
  CHECK_EQ(d.forecast().attack.damage, 1);  // now within 2 of the attacker
}

TEST(skills_forecast_notes_list_activations_in_roll_order) {
  Options o;
  o.aSkills = "\"heavy\", \"warded\", \"sure\"";
  o.dSkills = "\"hex\", \"shield\"";
  Duel d(skill("heavy", block("strike", "{ \"damagePercent\": 200 }", "", "\"a.skl\"")) + "," +
         skill("warded", block("struck", "{ \"damageTakenPercent\": 50 }", "", "40")) + "," +
         skill("sure", block("strike", "{ \"damage\": 2 }")) + "," +  // no chance: part of the numbers
         skill("hex", block("strike", "{ \"damage\": 1 }", "", "30")) + "," +
         skill("shield", block("struck", "{ \"damageTakenPercent\": 50 }", "", "50")) + "," +
         skill("never", block("strike", "{ \"damage\": 1 }", "", "0")), o);
  d.u(1).learned.push_back(d.skill("never"));
  const Forecast f = d.forecast();
  // Roll order of the fight: the attacker's strike (its own blocks, then the defender's "struck"), then the counter.
  CHECK_EQ(f.notes.size(), (size_t)4);
  const struct { int unit; const char* skill; int chance; } want[4] = {
      {0, "heavy", 20}, {1, "shield", 50}, {1, "hex", 30}, {0, "warded", 40}};
  for (size_t i = 0; i < f.notes.size() && i < 4; ++i) {
    CHECK_EQ(f.notes[i].unit, want[i].unit);
    CHECK_EQ(f.notes[i].skill, d.skill(want[i].skill));
    CHECK_EQ(f.notes[i].chance, want[i].chance);
  }
  CHECK_EQ(f.attack.damage, 4 + 2);  // "sure" is certain: in the numbers; the activations are not
  // Without a counterattack the defender's strike blocks and the attacker's "struck" blocks have no strike to judge.
  Options n = o;
  n.aSkills += ", \"quiet\"";
  Duel e(skill("heavy", block("strike", "{ \"damagePercent\": 200 }", "", "\"a.skl\"")) + "," +
         skill("hex", block("strike", "{ \"damage\": 1 }", "", "30")) + "," +
         skill("quiet", D("{ \"noCounter\": true }")) + "," + skill("warded", block("struck", "{ \"damageTaken\": 1 }", "", "40")) +
         "," + skill("sure", block("strike", "{ \"damage\": 2 }")) + "," + skill("shield", block("struck", "{ \"damageTaken\": 1 }", "", "50")), n);
  const Forecast g = e.forecast();
  CHECK_EQ(g.notes.size(), (size_t)2);  // heavy, shield
}
