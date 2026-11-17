// The battle scene's fight script (engine/player/battle/fight_script.*): the events of one fight turned into beats.
#include "battle/fight_script.h"
#include "test.h"

using namespace ote;

namespace {

Event strike(int a, int b, int flags, int damage, int weapon = 0) {
  Event e;
  e.type = EventType::Attack;
  e.a = a;
  e.b = b;
  e.c = weapon;
  e.d = damage;
  e.flags = flags;
  return e;
}
Event simple(EventType t, int a, int b = -1, int c = -1) {
  Event e;
  e.type = t;
  e.a = a;
  e.b = b;
  e.c = c;
  return e;
}

// Unit 1 "Eirik" (hp 20 of 24) at x 2, unit 2 "Gorm" (hp 10 of 30) at x 3; unit 7 is somebody else.
FightInfo info(int x1 = 2, int x2 = 3) {
  FightInfo f;
  f.units[1] = {1, "Eirik", 3, 20, 24, x1};
  f.units[2] = {2, "Gorm", 5, 10, 30, x2};
  f.units[7] = {7, "Other", 1, 5, 5, 0};
  f.skillNames = {"Heavy Blow", "Vengeance"};
  f.conditionNames = {"Poisoned"};
  f.itemNames = {"Iron Sword"};
  return f;
}

}  // namespace

TEST(fight_script_hit_and_counter) {
  std::vector<Event> run = {strike(1, 2, kHit, 6), strike(2, 1, kHit | kCounter, 4)};
  FightScript s = buildFightScript(run, info());
  CHECK(s.valid);
  CHECK_EQ((int)s.strikes.size(), 2);
  CHECK_EQ(s.side[0].unit, 1);  // the attacker stands further left
  CHECK_EQ(s.side[1].unit, 2);
  CHECK_EQ(s.initiator, 0);
  CHECK_EQ(s.strikes[0].actor, 0);
  CHECK(s.strikes[0].hit && !s.strikes[0].crit && !s.strikes[0].counter);
  CHECK_EQ(s.strikes[0].damage, 6);
  CHECK_EQ(s.strikes[0].hpBefore[1], 10);
  CHECK_EQ(s.strikes[0].hpAfterHit[1], 4);  // the foe's bar drains with the hit
  CHECK_EQ(s.strikes[0].hpAfterHit[0], 20);
  CHECK_EQ(s.strikes[1].actor, 1);
  CHECK(s.strikes[1].counter);
  CHECK_EQ(s.strikes[1].hpBefore[0], 20);
  CHECK_EQ(s.strikes[1].hpAfterHit[0], 16);
  CHECK_EQ(s.strikes[1].hpEnd[1], 4);  // hit points carry from strike to strike
}

TEST(fight_script_miss_crit_and_follow_up) {
  std::vector<Event> run = {strike(1, 2, 0, 0), strike(2, 1, kHit | kCounter | kCrit, 9), strike(1, 2, kHit | kCrit, 5)};
  FightScript s = buildFightScript(run, info());
  CHECK_EQ((int)s.strikes.size(), 3);
  CHECK(!s.strikes[0].hit && s.strikes[0].damage == 0);
  CHECK_EQ(s.strikes[0].hpAfterHit[1], 10);  // a miss changes nothing
  CHECK(s.strikes[1].crit && s.strikes[1].counter);
  CHECK_EQ(s.strikes[1].hpAfterHit[0], 11);
  CHECK(s.strikes[2].crit && !s.strikes[2].counter);  // the follow-up is the attacker's again
  CHECK_EQ(s.strikes[2].actor, 0);
  CHECK_EQ(s.strikes[2].hpEnd[1], 5);
  // A crit flag without a hit is not a crit.
  std::vector<Event> odd = {strike(1, 2, kCrit, 0)};
  CHECK(!buildFightScript(odd, info()).strikes[0].crit);
}

TEST(fight_script_kills_on_both_sides) {
  // The attacker kills: the foe's death follows its killing strike.
  std::vector<Event> run = {strike(1, 2, kHit | kKill, 12), simple(EventType::UnitDied, 2)};
  FightScript s = buildFightScript(run, info());
  CHECK_EQ((int)s.strikes.size(), 1);
  CHECK(s.strikes[0].kill && s.strikes[0].dies[1] && !s.strikes[0].dies[0]);
  CHECK_EQ(s.strikes[0].hpAfterHit[1], 0);
  // A counter kills the attacker.
  std::vector<Event> run2 = {strike(1, 2, kHit, 3), strike(2, 1, kHit | kKill | kCounter, 30), simple(EventType::UnitDied, 1)};
  FightScript s2 = buildFightScript(run2, info());
  CHECK(s2.strikes[1].dies[0] && !s2.strikes[1].dies[1]);
  CHECK(s2.strikes[1].counter && s2.strikes[1].kill);
  CHECK_EQ(s2.strikes[1].hpEnd[0], 0);
  // A death with no kill flag (recoil) still marks the fighter.
  std::vector<Event> run3 = {strike(1, 2, kHit, 1), simple(EventType::UnitHurt, 1, 50, 1), simple(EventType::UnitDied, 1)};
  FightScript s3 = buildFightScript(run3, info());
  CHECK(s3.strikes[0].dies[0]);
  CHECK_EQ(s3.strikes[0].hpEnd[0], 0);
  CHECK_EQ(s3.strikes[0].after[0].delta, -20);  // clamped to what it had left
}

TEST(fight_script_drain_recoil_and_survive) {
  std::vector<Event> run = {strike(1, 2, kHit, 8), simple(EventType::UnitHealed, 1, 6), strike(2, 1, kHit | kCounter, 2)};
  FightScript s = buildFightScript(run, info());
  CHECK_EQ((int)s.strikes[0].after.size(), 1);
  CHECK(s.strikes[0].after[0].side == 0);
  CHECK_EQ(s.strikes[0].hpEnd[0], 24);  // 20 + 6 would pass the maximum of 24: capped (6 -> 4 gained)
  CHECK_EQ(s.strikes[0].after[0].delta, 4);
  CHECK_EQ(s.strikes[1].hpBefore[0], 24);
  // Survive: "Survived!" is a tag on the struck fighter.
  std::vector<Event> run2 = {strike(1, 2, kHit | kSurvived, 9)};
  FightScript s2 = buildFightScript(run2, info());
  CHECK_EQ((int)s2.strikes[0].tags.size(), 1);
  CHECK(s2.strikes[0].tags[0].side == 1 && s2.strikes[0].tags[0].text == "Survived!");
  CHECK_EQ(s2.strikes[0].hpAfterHit[1], 1);
  // A heal with no strike before it (a skill after the fight) is not part of the scene's bars.
  std::vector<Event> run3 = {simple(EventType::UnitHealed, 1, 6), strike(1, 2, kHit, 2)};
  CHECK(buildFightScript(run3, info()).strikes[0].after.empty());
}

TEST(fight_script_skill_tags_belong_to_the_next_strike) {
  std::vector<Event> run = {simple(EventType::SkillActivated, 1, 0, 2), strike(1, 2, kHit, 4), simple(EventType::SkillActivated, 2, 1, 1),
                            strike(2, 1, kHit | kCounter, 3), simple(EventType::ConditionApplied, 2, 0, 1)};
  FightScript s = buildFightScript(run, info());
  CHECK_EQ((int)s.strikes.size(), 2);
  CHECK_EQ((int)s.strikes[0].tags.size(), 1);
  CHECK(s.strikes[0].tags[0].side == 0 && s.strikes[0].tags[0].text == "Heavy Blow");
  CHECK_EQ((int)s.strikes[1].tags.size(), 2);  // the foe's skill before its counter, then the condition after it
  CHECK(s.strikes[1].tags[0].side == 1 && s.strikes[1].tags[0].text == "Vengeance");
  CHECK(s.strikes[1].tags[1].side == 1 && s.strikes[1].tags[1].text == "Poisoned");
  // Unknown skill index: a plain word, never a crash.
  std::vector<Event> run2 = {simple(EventType::SkillActivated, 1, 99, 2), strike(1, 2, 0, 0)};
  CHECK(buildFightScript(run2, info()).strikes[0].tags[0].text == "Skill");
}

TEST(fight_script_weapon_break_and_resisted) {
  std::vector<Event> run = {strike(1, 2, kHit, 2), simple(EventType::ItemBroke, 1, 0), simple(EventType::ConditionResisted, 2, 0, 1)};
  FightScript s = buildFightScript(run, info());
  CHECK(s.strikes[0].weaponBroke && s.strikes[0].brokeText == "Iron Sword broke");
  CHECK_EQ((int)s.strikes[0].tags.size(), 1);
  CHECK(s.strikes[0].tags[0].text == "Poisoned resisted");
}

TEST(fight_script_sides_and_range) {
  // The unit nearer the left edge is on the left, whoever attacked.
  FightScript s = buildFightScript({strike(1, 2, kHit, 2)}, info(5, 3));
  CHECK_EQ(s.side[0].unit, 2);
  CHECK_EQ(s.side[1].unit, 1);
  CHECK_EQ(s.initiator, 1);
  CHECK_EQ(s.strikes[0].actor, 1);
  // The same column: the attacker is on the left.
  FightScript t = buildFightScript({strike(2, 1, kHit, 2)}, info(4, 4));
  CHECK(t.side[0].unit == 2 && t.initiator == 0);
  // Counter-only run (the foe's reply alone): the unit it struck started it.
  FightScript c = buildFightScript({strike(2, 1, kHit | kCounter, 2)}, info());
  CHECK(c.valid);
  CHECK_EQ(c.side[c.initiator].unit, 1);
  CHECK(c.strikes[0].counter);
  CHECK(!s.ranged());
  FightInfo far = info();
  far.distance = 3;
  CHECK(buildFightScript({strike(1, 2, kHit, 2)}, far).ranged());
  CHECK_EQ(buildFightScript({strike(1, 2, kHit, 2)}, far).distance, 3);
}

TEST(fight_script_no_strike_is_not_valid) {
  CHECK(!buildFightScript({}, info()).valid);
  CHECK(!buildFightScript({simple(EventType::SkillActivated, 1, 0)}, info()).valid);
}

TEST(fight_script_what_belongs_to_a_fight) {
  CHECK(belongsToFight(strike(1, 2, kHit, 3), 1, 2));
  CHECK(belongsToFight(strike(2, 1, kHit | kCounter, 3), 1, 2));
  CHECK(!belongsToFight(strike(1, 7, kHit, 3), 1, 2));  // another pair
  CHECK(belongsToFight(simple(EventType::SkillActivated, 2, 0, 1), 1, 2));
  CHECK(!belongsToFight(simple(EventType::SkillActivated, 7, 0, 1), 1, 2));
  CHECK(belongsToFight(simple(EventType::UnitDied, 2), 1, 2));
  CHECK(!belongsToFight(simple(EventType::UnitDied, 7), 1, 2));
  CHECK(belongsToFight(simple(EventType::UnitHealed, 1, 3), 1, 2));
  CHECK(!belongsToFight(simple(EventType::Dialogue, 1), 1, 2));  // a quote ends the run
  CHECK(!belongsToFight(simple(EventType::XpGained, 1, 5), 1, 2));
  CHECK(!belongsToFight(simple(EventType::LevelUp, 1, 4), 1, 2));
  CHECK(!belongsToFight(simple(EventType::UnitMoved, 1), 1, 2));
  CHECK(!belongsToFight(simple(EventType::UnitDied, -1), -1, 2));
}
