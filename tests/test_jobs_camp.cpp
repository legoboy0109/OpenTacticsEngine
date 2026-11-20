// docs/JOBS.md "Between chapters": changing a roster character's job, learning and equipping in the camp, what a job change does to the unit type and the
// stats, the roster carrying the job state across two chapters of the fixture campaign, and the roster in saves. Fixture tests/fixtures/jobs (jobs_fixture.h).
#include "core/campaign/campaign_save.h"
#include "jobs_fixture.h"

using namespace ote;
using skilltest::saysIn;

namespace {

// Chapter 1 played as far as the camp: Aldric strikes the dummy once (+10 JP: 40 in Squire), then the battle is won and the roster holds him and Bryn.
struct AfterChapterOne {
  jobtest::Copy c;
  jobtest::Game g;
  explicit AfterChapterOne(const char* name) : c(name) {
    CHECK(g.load(c.dir));
    CHECK(g.startChapter());
    Unit& aldric = g.unit("aldric");
    CHECK(g.battle.apply(Action::attack(g.battle.unitByStoryId("aldric"), g.battle.unitByStoryId("foe"), g.db.findWeapon("sword")), g.events));
    CHECK_EQ(aldric.jobs.find(g.job("squire"))->earned, 40);
    CHECK(g.winAndFinish());
  }
  RosterUnit& r(const char* id) { return *g.rosterUnit(id); }
  std::string why;
  bool change(const char* who, const char* job) { return jobsChange(g.db, g.pt, who, g.job(job), why); }
  bool learn(const char* who, const char* what) { return jobsLearn(g.db, g.pt, who, g.entry(what), why); }
  bool equip(const char* who, const char* what) { return jobsEquip(g.db, g.pt, who, g.entry(what), why); }
};

std::vector<int> statsOf(const RosterUnit& r) { return r.stats; }

}  // namespace

TEST(the_roster_takes_the_job_state_of_a_won_battle_over) {
  AfterChapterOne a("jc_roster");
  CHECK_EQ(a.g.pt.chapter, 1);
  CHECK(a.g.pt.phase == Phase::Before);
  RosterUnit& aldric = a.r("aldric");
  CHECK_EQ(aldric.jobs.job, a.g.job("squire"));
  CHECK_EQ(aldric.jobs.find(a.g.job("squire"))->earned, 40);
  CHECK_EQ(aldric.type, a.g.db.findUnitType("squire"));
  CHECK_EQ(a.r("bryn").jobs.job, a.g.job("squire"));
  CHECK(a.r("bryn").jobs.progress.empty());  // no JP: Bryn did not fight
  CHECK_EQ((int)jobsRosterIds(a.g.db, a.g.pt).size(), 2);
}

TEST(a_job_change_moves_the_stats_by_the_difference_of_the_unit_types_and_back) {
  AfterChapterOne a("jc_change");
  RosterUnit& aldric = a.r("aldric");
  const std::vector<int> before = statsOf(aldric);  // squire: hp 30 str 6 def 2 mov 4
  CHECK(a.change("aldric", "knight"));  // knight: hp 36 str 7 def 5 mov 3
  CHECK_EQ(aldric.type, a.g.db.findUnitType("knight"));
  CHECK_EQ(aldric.jobs.job, a.g.job("knight"));
  CHECK_EQ(aldric.stats[(size_t)a.g.db.findStat("hp")], 36);
  CHECK_EQ(aldric.stats[(size_t)a.g.db.findStat("str")], 7);
  CHECK_EQ(aldric.stats[(size_t)a.g.db.findStat("def")], 5);
  CHECK_EQ(aldric.stats[(size_t)a.g.db.findStat("mov")], 3);
  CHECK_EQ(aldric.level, 1);  // level, experience and items are the character's, not the job's
  CHECK(a.change("aldric", "squire"));
  CHECK(aldric.stats == before);  // the way back is exact
  CHECK_EQ(aldric.type, a.g.db.findUnitType("squire"));
  // Own stat gains (a level up's) stay across the change: they are part of the character.
  aldric.stats[(size_t)a.g.db.findStat("str")] += 3;
  CHECK(a.change("aldric", "mage"));
  CHECK_EQ(aldric.stats[(size_t)a.g.db.findStat("str")], 6);  // 9 + (3 - 6)
}

TEST(a_job_change_is_refused_for_a_locked_job_the_same_job_and_unknown_things) {
  AfterChapterOne a("jc_refuse");
  CHECK(!a.change("bryn", "knight"));  // Bryn has no JP: Squire level 1
  CHECK_STR(a.why, "needs Squire level 2");
  CHECK_EQ(a.r("bryn").jobs.job, a.g.job("squire"));
  CHECK(!a.change("aldric", "squire"));
  CHECK(saysIn(a.why, "is a Squire already"));
  CHECK(!a.change("nobody", "knight"));
  CHECK_STR(a.why, "there is no such character");
  CHECK(!jobsChange(a.g.db, a.g.pt, "aldric", 9, a.why));
  CHECK_STR(a.why, "there is no such job");
  // A refusal changes nothing.
  CHECK_EQ(a.r("bryn").type, a.g.db.findUnitType("squire"));
  CHECK(a.r("bryn").stats[(size_t)a.g.db.findStat("def")] == 2);
}

TEST(learning_and_equipping_in_the_camp_follow_the_rules_of_the_state) {
  AfterChapterOne a("jc_learn");
  CHECK(a.learn("aldric", "guard"));  // 30 of the 40 JP
  CHECK_EQ(jobUnspent(a.r("aldric").jobs, a.g.job("squire")), 10);
  CHECK(!a.learn("aldric", "shove"));
  CHECK(saysIn(a.why, "costs 40 JP, 10 left in Squire"));
  CHECK(!a.equip("aldric", "guard"));  // it is the current job's own: active anyway
  CHECK(saysIn(a.why, "belongs to the current job"));
  CHECK(a.change("aldric", "knight"));
  CHECK(a.equip("aldric", "guard"));
  CHECK_EQ((int)a.r("aldric").jobs.equipped.size(), 1);
  CHECK(a.change("aldric", "squire"));  // Guard is Squire's own again: it leaves the equipped list
  CHECK(a.r("aldric").jobs.equipped.empty());
  CHECK(!jobsLearn(a.g.db, a.g.pt, "ghost", a.g.entry("guard"), a.why));
  CHECK(!jobsEquip(a.g.db, a.g.pt, "ghost", a.g.entry("guard"), a.why));
  CHECK(jobsChangeAllowedInPrep(a.g.db));
}

TEST(the_roster_carries_job_type_stats_and_equipment_into_the_next_chapter) {
  AfterChapterOne a("jc_carry");
  CHECK(a.learn("aldric", "guard"));
  CHECK(a.change("aldric", "knight"));
  CHECK(a.equip("aldric", "guard"));
  CHECK(a.g.startChapter());  // chapter 2: Aldric, Bryn and the new Cora against the shaman
  Unit& aldric = a.g.unit("aldric");
  CHECK_EQ(aldric.type, a.g.db.findUnitType("knight"));
  CHECK_EQ(aldric.jobs.job, a.g.job("knight"));
  CHECK(aldric.jobs == a.r("aldric").jobs);
  CHECK_EQ(a.g.battle.stat(aldric, a.g.db.findStat("def")), 5);
  CHECK_EQ(aldric.hp, 36);  // a chapter always begins at full strength
  std::vector<int> skills;
  a.g.battle.skillsOf(aldric, -1, skills);
  CHECK_EQ((int)skills.size(), 1);  // Guard, equipped from Squire
  CHECK_STR(a.g.db.skills[(size_t)skills[0]].id, "guard");
  // The newcomer comes in as her character describes her: a Mage with Guard equipped.
  Unit& cora = a.g.unit("cora");
  CHECK_EQ(cora.jobs.job, a.g.job("mage"));
  CHECK_EQ(cora.type, a.g.db.findUnitType("mage"));
  // Bryn is still the Squire he was, and the enemy shaman is built from its map entry, not the roster.
  CHECK_EQ(a.g.unit("bryn").jobs.job, a.g.job("squire"));
  CHECK_EQ(a.g.unit("shaman").jobs.job, a.g.job("mage"));
  // JP earned in chapter 2 goes to the job Aldric uses now (Knight), and the roster has it afterwards.
  CHECK(a.g.battle.apply(Action::move(a.g.battle.unitByStoryId("aldric"), a.g.battle.grid.at(4, 2)), a.g.events));
  CHECK(a.g.battle.apply(Action::attack(a.g.battle.unitByStoryId("aldric"), a.g.battle.unitByStoryId("shaman"), a.g.db.findWeapon("sword")), a.g.events));
  CHECK_EQ(aldric.jobs.find(a.g.job("knight"))->earned, 10);
  CHECK_EQ(aldric.jobs.find(a.g.job("squire"))->earned, 40);
  CHECK(a.g.winAndFinish());
  CHECK_EQ(a.r("aldric").jobs.find(a.g.job("knight"))->earned, 10);
  CHECK(a.g.rosterUnit("cora") != nullptr);  // Cora joined the roster with her character's job state
  if (!a.g.rosterUnit("cora")) return;
  CHECK_EQ(a.r("cora").jobs.job, a.g.job("mage"));
}

TEST(the_roster_with_jobs_goes_through_a_save_and_a_damaged_one_is_refused) {
  AfterChapterOne a("jc_save");
  CHECK(a.learn("aldric", "guard"));
  CHECK(a.change("aldric", "knight"));
  CHECK(a.equip("aldric", "guard"));
  Json j;
  savePlaythrough(a.g.db, a.g.pt, j);
  const std::string text = j.dump(0);
  CHECK(text.find("\"jobs\"") != std::string::npos);
  Playthrough back;
  std::string err;
  CHECK(restorePlaythrough(a.g.db, j, back, err));
  if (!err.empty()) std::printf("  %s\n", err.c_str());
  Json again;
  savePlaythrough(a.g.db, back, again);
  CHECK_STR(again.dump(0), text);
  // Damaged copies.
  auto tryWith = [&](const char* from, const char* to) -> std::string {
    std::string t = text;
    const size_t at = t.find(from);
    if (at == std::string::npos) {
      std::printf("  the test's text to replace is missing: %s\n", from);
      return "(missing)";
    }
    t.replace(at, std::string(from).size(), to);
    Json bad;
    std::string perr;
    if (!Json::parse(t, bad, perr)) return "(parse) " + perr;
    Playthrough out;
    return restorePlaythrough(a.g.db, bad, out, perr) ? "" : perr;
  };
  CHECK(saysIn(tryWith("\"jobs\":{\"job\":1", "\"jobs\":{\"job\":7"), "playthrough.roster[0].jobs.job: value 7 is outside 0..2"));
  CHECK(saysIn(tryWith("\"spent\":30", "\"spent\":50"), "spent JP must be from 0 to the JP earned"));
  CHECK(saysIn(tryWith("\"equipped\":[0]", "\"equipped\":[5]"), "has not been learned"));
}
