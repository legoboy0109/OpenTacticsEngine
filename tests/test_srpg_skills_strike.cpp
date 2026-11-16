// Step K2 of docs/SRPG.md: "strike" and "struck" blocks with real skills, and the random numbers they use. These
// repeat F2's stand-in tests (test_srpg_combat_rolls.cpp) with skills from data: roll order, no roll at 0 or 100,
// SkillActivated before the Attack event. Duel: tests/srpg_skills_duel.h.
#include "srpg_skills_duel.h"

using namespace ote;
using namespace duel;

namespace {

std::string S(const std::string& effects, const std::string& chance = "", const std::string& cond = "") {
  return block("strike", effects, cond, chance);
}
std::string T(const std::string& effects, const std::string& chance = "", const std::string& cond = "") {
  return block("struck", effects, cond, chance);
}

// Index of the first event of the type, -1 if none.
int indexOf(const Duel& d, EventType t) {
  for (size_t i = 0; i < d.w.events.size(); ++i)
    if (d.w.events[i].type == t) return (int)i;
  return -1;
}

}  // namespace

TEST(skills_strike_activations_roll_in_list_order_and_before_the_hit) {
  // Two activations at 50 %: "big" (+10 damage) and "feint" (the strike does nothing). The attacker strikes once
  // (the 100 % hit and 0 % crit roll nothing), so exactly two numbers are used, in list order.
  const std::string skills = skill("big", S("{ \"damage\": 10 }", "50")) + "," + skill("feint", S("{ \"damagePercent\": 0 }", "50"));
  int differ = 0;
  for (uint32_t seed = 1; seed <= 30; ++seed) {
    int result[2];
    for (int order = 0; order < 2; ++order) {
      Options o;
      o.seed = seed;
      o.aSkills = order == 0 ? "\"big\", \"feint\"" : "\"feint\", \"big\"";
      Duel d(skills, o);
      d.u(1).hp = 30;
      Rng r = d.b().rng;  // the same stream, rolled by hand
      const bool first = r.chance(50), second = r.chance(50);
      CHECK(d.attack());
      CHECK(d.b().rng.state == r.state);  // two numbers used, no more, no fewer
      const bool big = order == 0 ? first : second, feint = order == 0 ? second : first;
      CHECK_EQ(30 - d.hp(1), feint ? 0 : 4 + (big ? 10 : 0));
      result[order] = 30 - d.hp(1);
    }
    differ += result[0] != result[1];
  }
  CHECK(differ > 0);  // on some seed the order decided the fight
}

TEST(skills_strike_chance_of_0_or_100_rolls_nothing) {
  Options o;
  o.aSkills = "\"never\", \"always\"";
  o.dSkills = "\"also\"";
  Duel d(skill("never", S("{ \"damage\": 10 }", "0")) + "," + skill("always", S("{ \"damage\": 5 }", "100")) + "," +
         skill("also", T("{ \"damageTaken\": -1 }", "250")), o);
  const uint64_t before = d.b().rng.state;
  CHECK(d.attack());
  CHECK(d.b().rng.state == before);
  CHECK_EQ(30 - d.hp(1), 4 + 5 - 1);  // "never" did nothing, "always" and "also" applied
  CHECK_EQ(d.w.count(EventType::SkillActivated), 2);
}

TEST(skills_strike_activation_events_come_just_before_the_attack_event) {
  Options o;
  o.aSkills = "\"a1\"";
  o.dSkills = "\"d1\"";
  Duel d(skill("a1", S("{ \"damage\": 1 }", "100")) + "," + skill("d1", T("{ \"damageTaken\": -1 }", "100")), o);
  CHECK(d.attack());
  // A's strike: its activation, the struck unit's activation (the strike hit), the Attack; then the counter, which
  // has no activations of its own ("d1" is a "struck" block of unit 1, "a1" of unit 0 is a "strike" block).
  CHECK_STR(d.story(), "ssAD");
  const Event& s0 = d.w.events[(size_t)indexOf(d, EventType::SkillActivated)];
  CHECK_EQ(s0.a, 0);
  CHECK_EQ(s0.b, d.skill("a1"));
  CHECK_EQ(s0.c, 1);
  CHECK(indexOf(d, EventType::SkillActivated) < indexOf(d, EventType::Attack));
}

TEST(skills_strike_activation_shows_on_a_miss_but_a_struck_one_only_on_a_hit) {
  // Unit 1 dodges everything (avoid 100 from a combat block): the attacker's activation is announced, the struck
  // unit's is not (the strike never hit it).
  Options o;
  o.aSkills = "\"a1\"";
  o.dSkills = "\"d1\", \"dodge\"";
  Duel d(skill("a1", S("{ \"damage\": 1 }", "100")) + "," + skill("d1", T("{ \"damageTaken\": -1 }", "100")) + "," +
         skill("dodge", block("combat", "{ \"avoid\": 100 }")), o);
  CHECK(d.attack());
  CHECK_STR(d.story(), "saD");
  CHECK_EQ(d.hp(1), 30);
}

TEST(skills_strike_drain_heals_after_the_attack_event) {
  Options o;
  o.aSkills = "\"leech\"";
  Duel d(skill("leech", S("{ \"drain\": 50 }")), o);
  d.u(0).hp = 20;
  CHECK(d.attack());
  CHECK_STR(d.story(), "AhD");  // no chance: passive, no SkillActivated
  CHECK_EQ(d.hp(0), 20 + 2 - 4);
}

TEST(skills_strike_again_gives_one_more_strike_that_cannot_give_another) {
  Options o;
  o.aSkills = "\"again\"";
  Duel d(skill("again", S("{ \"strikeAgain\": true }", "100")), o);
  CHECK(d.attack());
  CHECK_STR(d.story(), "sAsAD");  // the extra strike rolls its own activations but cannot grant a third strike
  CHECK_EQ(d.hp(1), 30 - 8);
}

TEST(skills_struck_survive_cuts_a_killing_blow_to_one_hp) {
  Options o;
  o.aSkills = "\"smash\"";
  o.dSkills = "\"hang\"";
  Duel d(skill("smash", S("{ \"damage\": 40 }")) + "," + skill("hang", T("{ \"survive\": true }")), o);
  d.u(1).hp = 9;
  CHECK(d.attack());
  CHECK_EQ(d.hp(1), 1);
  CHECK(d.u(1).alive);
  bool flagged = false;
  for (const Event& e : d.w.events) flagged |= e.type == EventType::Attack && e.a == 0 && (e.flags & kSurvived);
  CHECK(flagged);
  // At 1 HP there is nothing to leave: the next blow defeats it.
  d.u(0).done = false;
  CHECK(d.attack());
  CHECK(!d.u(1).alive);
}

TEST(skills_strike_percents_run_the_strikers_first_then_the_struck_units) {
  Options o;
  o.aSkills = "\"p150\"";
  o.dSkills = "\"p90\"";
  Duel d(skill("p150", S("{ \"damagePercent\": 150 }")) + "," + skill("p90", T("{ \"damageTakenPercent\": 90 }")), o);
  CHECK_EQ(d.forecast().attack.damage, 5);  // passives are in the forecast: 4 * 150 / 100 = 6, 6 * 90 / 100 = 5
  CHECK(d.attack());
  CHECK_EQ(d.hp(1), 25);
}

TEST(skills_strike_weapon_skills_and_defender_without_an_answering_weapon) {
  // "weapon_skill" belongs to the hexed sword: judged for the weapon used in this fight, here the attacker's.
  Options o;
  o.aWeapon = "hexed_sword";
  Duel d(skill("weapon_skill", S("{ \"damage\": 3 }")), o);
  CHECK(d.attack());
  CHECK_EQ(d.hp(1), 30 - 7);
  CHECK_EQ(d.hp(0), 30 - 4);
}

TEST(skills_activations_repeat_with_the_same_seed_exactly) {
  auto fight = [](uint32_t seed) {
    Options o;
    o.seed = seed;
    o.aSkills = "\"big\", \"again\"";
    o.dSkills = "\"ward\"";
    Duel d(skill("big", S("{ \"damagePercent\": 200 }", "\"a.skl * 2\"")) + "," + skill("again", S("{ \"strikeAgain\": true }", "35")) +
           "," + skill("ward", T("{ \"damageTakenPercent\": 50 }", "45")), o);
    d.u(0).hp = d.u(1).hp = 400;
    std::string out;
    for (int i = 0; i < 6; ++i) {
      d.u(0).done = false;
      d.attack();
      out += d.story() + "|";
    }
    return out + std::to_string(d.hp(0)) + "," + std::to_string(d.hp(1)) + "," + std::to_string(d.b().rng.state);
  };
  CHECK_STR(fight(3), fight(3));
  CHECK(fight(3) != fight(4));
}
