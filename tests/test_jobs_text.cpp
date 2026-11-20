// docs/JOBS.md "Player UI": the rows of the Jobs screen's three lists and the character list, and what the unit details say about a unit's job (the Job row of
// the Stats page, the "Job" origin of a skill). The player's screens only show these; nothing here needs a window. Fixture tests/fixtures/jobs (jobs_fixture.h).
#include "battle/unit_info_rows.h"
#include "jobs/jobs_text.h"
#include "jobs_fixture.h"

using namespace ote;
using skilltest::saysIn;

namespace {

struct Camp {
  jobtest::Copy c;
  jobtest::Game g;
  explicit Camp(const char* name) : c(name) {
    CHECK(g.load(c.dir));
    CHECK(g.startChapter());
    CHECK(g.battle.apply(Action::attack(g.battle.unitByStoryId("aldric"), g.battle.unitByStoryId("foe"), g.db.findWeapon("sword")), g.events));
    CHECK(g.winAndFinish());
  }
  std::string why;
};

}  // namespace

TEST(the_jobs_tab_marks_the_current_job_and_greys_a_locked_one_with_its_reason) {
  Camp a("jt_jobs");
  const JobRows bryn = jobsTabRows(a.g.db, *a.g.rosterUnit("bryn"));
  CHECK_EQ((int)bryn.rows.size(), 3);
  CHECK_STR(bryn.rows[0].label, "Squire");
  CHECK(bryn.rows[0].mark && !bryn.rows[0].dim);
  CHECK_STR(bryn.rows[0].extra, "Current Lv 1");
  CHECK(bryn.rows[1].dim);
  CHECK_STR(bryn.rows[1].extra, "Locked");
  CHECK(saysIn(bryn.rows[1].detail, "Locked: needs Squire level 2."));
  const JobRows aldric = jobsTabRows(a.g.db, *a.g.rosterUnit("aldric"));
  CHECK(!aldric.rows[1].dim);  // 40 JP in Squire: level 2
  CHECK_STR(aldric.rows[1].extra, "Lv 1");
  CHECK(saysIn(aldric.rows[1].detail, "Plays as a Knight."));
  CHECK_EQ(aldric.ref[2], a.g.job("mage"));
}

TEST(the_learn_tab_lists_costs_greys_what_cannot_be_afforded_and_says_why) {
  Camp a("jt_learn");
  const JobRows rows = learnTabRows(a.g.db, *a.g.rosterUnit("aldric"));
  CHECK(rows.rows[0].header);
  CHECK_EQ(rows.ref[0], -1);
  CHECK(saysIn(rows.rows[0].label, "Squire  Lv 2  JP 40"));
  CHECK_STR(rows.rows[1].label, "Guard");
  CHECK_STR(rows.rows[1].extra, "30 JP");
  CHECK(!rows.rows[1].dim);
  CHECK_STR(rows.rows[2].label, "Shove (A)");
  CHECK(!rows.rows[2].dim);  // 40 of 40
  CHECK(saysIn(rows.rows[1].detail, "Skill, support slot. Takes 2 less from every hit."));
  // The Knight's heading says it has no JP and its entry cannot be afforded; Mage the same.
  bool sawKnight = false;
  for (size_t i = 0; i < rows.rows.size(); ++i)
    if (rows.rows[i].header && rows.rows[i].label.find("Knight") == 0) {
      sawKnight = true;
      CHECK(rows.rows[i + 1].dim);
      CHECK(saysIn(rows.rows[i + 1].detail, "Costs 60 JP, 0 left in Knight."));
    }
  CHECK(sawKnight);
  CHECK(jobsLearn(a.g.db, a.g.pt, "aldric", a.g.entry("guard"), a.why));
  const JobRows after = learnTabRows(a.g.db, *a.g.rosterUnit("aldric"));
  CHECK_STR(after.rows[1].extra, "Learned");
  CHECK(after.rows[1].mark && after.rows[1].dim);
  // A locked job's heading says so, and its entries stay greyed whatever the JP.
  const JobRows locked = learnTabRows(a.g.db, *a.g.rosterUnit("bryn"));
  bool sawLocked = false;
  for (const PanelRow& r : locked.rows) sawLocked = sawLocked || (r.header && r.label.find("(locked)") != std::string::npos);
  CHECK(sawLocked);
}

TEST(the_equip_tab_lists_what_other_jobs_taught_under_its_slot_kind) {
  Camp a("jt_equip");
  CHECK(equipTabRows(a.g.db, *a.g.rosterUnit("aldric")).rows.empty());  // nothing learned yet
  CHECK(jobsLearn(a.g.db, a.g.pt, "aldric", a.g.entry("guard"), a.why));
  CHECK(equipTabRows(a.g.db, *a.g.rosterUnit("aldric")).rows.empty());  // it is the current job's own
  CHECK(jobsChange(a.g.db, a.g.pt, "aldric", a.g.job("knight"), a.why));
  JobRows rows = equipTabRows(a.g.db, *a.g.rosterUnit("aldric"));
  CHECK_EQ((int)rows.rows.size(), 2);
  CHECK(rows.rows[0].header);
  CHECK_STR(rows.rows[0].label, "Support  0/1");
  CHECK_STR(rows.rows[1].label, "Guard");
  CHECK_STR(rows.rows[1].extra, "Squire");
  CHECK_EQ(rows.ref[1], a.g.entry("guard"));
  CHECK(jobsEquip(a.g.db, a.g.pt, "aldric", a.g.entry("guard"), a.why));
  rows = equipTabRows(a.g.db, *a.g.rosterUnit("aldric"));
  CHECK_STR(rows.rows[0].label, "Support  1/1");
  CHECK_STR(rows.rows[1].extra, "Equipped");
  CHECK(rows.rows[1].mark);
}

TEST(the_character_list_names_each_character_with_its_job_and_level) {
  Camp a("jt_who");
  std::vector<std::string> ids;
  const std::vector<PanelRow> rows = jobsWhoRows(a.g.db, a.g.pt, ids);
  CHECK_EQ((int)rows.size(), 2);
  CHECK_STR(ids[0], "aldric");
  CHECK_STR(rows[0].label, "Aldric");
  CHECK_STR(rows[0].extra, "Squire 2");
  CHECK(saysIn(rows[0].detail, "Squire  Lv 2  JP 40"));
  CHECK_STR(rows[1].extra, "Squire 1");
  CHECK_STR(jobsSummary(a.g.db, *a.g.rosterUnit("bryn")), "Squire  Lv 1  JP 0");
}

TEST(the_unit_details_say_the_job_and_mark_its_skills_as_coming_from_the_job) {
  Camp a("jt_info");
  CHECK(jobsLearn(a.g.db, a.g.pt, "aldric", a.g.entry("guard"), a.why));
  CHECK(a.g.startChapter());
  Unit& aldric = a.g.unit("aldric");
  bool jobRow = false;
  for (const InfoRow& r : statRows(a.g.battle, aldric))
    if (r.label == "Job") {
      jobRow = true;
      CHECK_STR(r.value, "Squire  Lv 2");
      CHECK_STR(r.note, "JP 10");
    }
  CHECK(jobRow);
  const std::vector<SkillRow> skills = skillRows(a.g.battle, aldric);
  CHECK_EQ((int)skills.size(), 1);
  CHECK_STR(skills[0].name, "Guard");
  CHECK(skills[0].source == SkillSource::Job);
  CHECK_STR(skillSourceName(skills[0].source), "Job");
  // A unit without a job has no Job row.
  Unit& shaman = a.g.unit("shaman");
  CHECK(shaman.jobs.on());  // (enemies with a job show it too)
  for (const InfoRow& r : statRows(a.g.battle, a.g.unit("buddy"))) CHECK(r.label != "Job");
}
