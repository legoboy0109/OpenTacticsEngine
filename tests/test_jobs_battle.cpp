// docs/JOBS.md "In a battle": a unit with a job plays as the job's unit type, the skills and abilities its job taught it (and the ones it equipped) are
// its own, JP is earned wherever experience is awarded, and the whole state is saved. Fixture tests/fixtures/jobs, map "duel":
// Aldric (Squire, 30 JP in it) at 2,2 beside the dummy "foe" at 3,2, Cora (Mage, Guard equipped) at 1,0, a plain squire "plain" at 1,4 (no job), the grunt
// "weak" at 5,3 and the red shaman (a Mage that knows Mend) at 6,3 next to it.
#include "core/battle/save.h"
#include "jobs_fixture.h"

using namespace ote;
using skilltest::saysIn;

namespace {

struct Duel {
  jobtest::Copy c;
  skilltest::World w;
  explicit Duel(const char* name) : c(name), w("duel", c.dir) {}
  Battle& b() { return w.b(); }
  Unit& unit(const char* id) { return w.unit(id); }
  int entry(const char* id) const {
    for (const JobLearnDef& e : w.rig.db.jobEntries)
      if (e.refId == id) return e.flat;
    return -1;
  }
  int job(const char* id) const { return w.rig.db.findJob(id); }
  bool strike(const char* who, const char* target, int fromX = -1, int fromY = -1) {
    if (fromX >= 0 && !w.apply(Action::move(w.id(who), w.at(fromX, fromY)))) return false;
    return w.apply(Action::attack(w.id(who), w.id(target), w.rig.db.findWeapon("sword")));
  }
  std::vector<const Event*> jp() const {
    std::vector<const Event*> out;
    for (const Event& e : w.events)
      if (e.type == EventType::JpGained) out.push_back(&e);
    return out;
  }
};

}  // namespace

TEST(a_unit_with_a_job_plays_as_the_jobs_unit_type_with_the_jobs_skills_and_abilities) {
  Duel d("jb_play");
  const Database& db = d.w.db();
  Unit& aldric = d.unit("aldric");
  CHECK_EQ(aldric.type, db.findUnitType("squire"));
  CHECK_EQ(aldric.jobs.job, d.job("squire"));
  CHECK_EQ(d.b().unitJobLevel(aldric), 2);  // 30 JP in the squire table [20, 60, 120]
  CHECK_EQ(d.w.stat(aldric, "str"), 6);
  Unit& cora = d.unit("cora");
  CHECK_EQ(cora.type, db.findUnitType("mage"));  // her job decides the unit type
  CHECK_EQ(d.w.stat(cora, "str"), 3);
  CHECK_STR(d.w.skillIds(cora), "guard");  // Guard is learned in Squire and equipped in the support slot
  CHECK(d.b().jobGivesSkill(cora, d.w.skill("guard")));
  CHECK(!d.b().jobGivesSkill(aldric, d.w.skill("guard")));
  // The enemy shaman is a Mage by its job (no "type" on the map), knows Mend and has no skill.
  Unit& shaman = d.unit("shaman");
  CHECK_EQ(shaman.type, db.findUnitType("mage"));
  std::vector<int> abilities;
  d.b().abilitiesOf(shaman, abilities);
  CHECK_EQ((int)abilities.size(), 1);
  CHECK_STR(db.abilities[(size_t)abilities[0]].id, "mend");
  CHECK(d.b().jobGivesAbility(shaman, abilities[0]));
  d.b().abilitiesOf(aldric, abilities);
  CHECK(abilities.empty());
  // A unit without a job does not use the system.
  Unit& plain = d.unit("plain");
  CHECK_EQ(plain.jobs.job, -1);
  CHECK_EQ(d.b().unitJobLevel(plain), 0);
}

TEST(what_the_job_taught_is_active_and_what_is_equipped_adds_to_it) {
  Duel d("jb_skills");
  Unit& aldric = d.unit("aldric");
  std::string why;
  aldric.jobs.at(d.job("squire")).earned = 200;
  CHECK(jobLearn(d.w.db(), aldric.jobs, d.entry("guard"), why));
  CHECK_STR(d.w.skillIds(aldric), "guard");  // the current job's learned skills are active at once
  CHECK(jobLearn(d.w.db(), aldric.jobs, d.entry("shove"), why));
  std::vector<int> abilities;
  d.b().abilitiesOf(aldric, abilities);
  CHECK_EQ((int)abilities.size(), 1);
  // The same unit as a Mage: Squire's skills only count where they are equipped; Guard stays, Shove (an action entry) is not equipped.
  aldric.jobs.at(d.job("mage")).earned = 100;
  CHECK(jobSwitch(d.w.db(), aldric.jobs, d.job("mage"), why));
  CHECK_STR(d.w.skillIds(aldric), "");
  CHECK(jobEquip(d.w.db(), aldric.jobs, d.entry("guard"), why));
  CHECK_STR(d.w.skillIds(aldric), "guard");
  CHECK(jobLearn(d.w.db(), aldric.jobs, d.entry("mend"), why));
  d.b().abilitiesOf(aldric, abilities);
  CHECK_EQ((int)abilities.size(), 1);
  CHECK_STR(d.w.db().abilities[(size_t)abilities[0]].id, "mend");
  // Equipped entries join the unit's skills after its own, in the place the rules list them.
  CHECK(jobLearn(d.w.db(), aldric.jobs, d.entry("focus"), why));
  CHECK_STR(d.w.skillIds(aldric), "focus guard");
}

TEST(jp_is_earned_where_experience_is_awarded_by_a_unit_with_a_job_of_a_human_side) {
  Duel d("jb_jp");
  Unit& aldric = d.unit("aldric");
  CHECK(d.strike("aldric", "foe"));
  const std::vector<const Event*> jp = d.jp();
  CHECK_EQ((int)jp.size(), 1);  // the dummy is the computer's: no JP for it, and the plain squire is not involved
  if (!jp.empty()) {
    CHECK_EQ(jp[0]->a, d.w.id("aldric"));
    CHECK_EQ(jp[0]->b, 10);
    CHECK_EQ(jp[0]->c, d.job("squire"));
    CHECK_EQ(jp[0]->d, 2);
    CHECK_EQ(jp[0]->flags & kJobLevelUp, 0);
  }
  CHECK_EQ(aldric.jobs.find(d.job("squire"))->earned, 40);
  CHECK_EQ(jobUnspent(aldric.jobs, d.job("squire")), 40);
  // A unit without a job earns nothing.
  CHECK(d.w.apply(Action::move(d.w.id("plain"), d.w.at(3, 3))));
  CHECK(d.w.apply(Action::attack(d.w.id("plain"), d.w.id("foe"), d.w.rig.db.findWeapon("sword"))));
  CHECK_EQ((int)d.jp().size(), 0);
}

TEST(a_defeat_pays_extra_and_reaching_a_job_level_says_so) {
  Duel d("jb_kill");
  Unit& aldric = d.unit("aldric");
  d.unit("weak").hp = 5;
  aldric.jobs.at(d.job("squire")).earned = 55;
  CHECK(d.strike("aldric", "weak", 4, 3));
  int total = 0, levelUps = 0;
  for (const Event* e : d.jp()) {
    total += e->b;
    levelUps += (e->flags & kJobLevelUp) ? 1 : 0;
  }
  CHECK_EQ(total, 30);  // 20 for the defeat and 10 for the fight
  CHECK_EQ(levelUps, 1);
  CHECK_EQ(aldric.jobs.find(d.job("squire"))->earned, 85);
  CHECK_EQ(d.b().unitJobLevel(aldric), 3);
  CHECK_EQ(d.jp().back()->d, 3);
}

TEST(jp_goes_to_the_job_the_unit_uses_now) {
  Duel d("jb_where");
  Unit& cora = d.unit("cora");  // a Mage with 10 JP in it and 25 in Squire
  CHECK(d.w.apply(Action::move(d.w.id("cora"), d.w.at(1, 2))));
  CHECK(d.w.apply(Action::attack(d.w.id("cora"), d.w.id("foe"), d.w.rig.db.findWeapon("rod"))));  // the rod reaches two squares
  CHECK_EQ(cora.jobs.find(d.job("mage"))->earned, 20);
  CHECK_EQ(cora.jobs.find(d.job("squire"))->earned, 25);
}

TEST(job_state_is_saved_with_the_unit_and_comes_back_whole) {
  Duel d("jb_save");
  Unit& aldric = d.unit("aldric");
  std::string why;
  aldric.jobs.at(d.job("squire")).earned = 200;
  CHECK(jobLearn(d.w.db(), aldric.jobs, d.entry("guard"), why));
  CHECK(d.strike("aldric", "foe"));
  const std::string text = savetest::stateText(d.b());
  CHECK(text.find("\"jobs\"") != std::string::npos);
  Json j;
  saveBattleState(d.b(), j);
  skilltest::World fresh("duel", d.c.dir, false);
  std::string err;
  CHECK(restoreBattleState(fresh.b(), j, err));
  if (!err.empty()) std::printf("  %s\n", err.c_str());
  CHECK_STR(savetest::stateText(fresh.b()), text);
  CHECK(fresh.unit("aldric").jobs == d.unit("aldric").jobs);
  CHECK(fresh.unit("cora").jobs == d.unit("cora").jobs);
  CHECK_EQ(fresh.unit("plain").jobs.job, -1);  // the unit without a job writes nothing
}

TEST(a_save_with_inconsistent_job_data_is_refused_in_the_usual_words) {
  Duel d("jb_bad");
  Json good;
  saveBattleState(d.b(), good);
  const std::string text = good.dump(0);
  auto tryWith = [&](const char* from, const char* to) -> std::string {
    std::string t = text;
    const size_t at = t.find(from);
    if (at == std::string::npos) {
      std::printf("  the test's text to replace is missing: %s\n", from);
      return "(missing)";
    }
    t.replace(at, std::string(from).size(), to);
    Json j;
    std::string perr;
    if (!Json::parse(t, j, perr)) return "(parse) " + perr;
    skilltest::World fresh("duel", d.c.dir, false);
    std::string err;
    return restoreBattleState(fresh.b(), j, err) ? "" : err;
  };
  CHECK_STR(tryWith("\"earned\":30", "\"earned\":30"), "");
  CHECK(saysIn(tryWith("\"jobs\":{\"job\":0", "\"jobs\":{\"job\":9"), "units[0].jobs.job: value 9 is outside 0..2"));
  CHECK(saysIn(tryWith("\"earned\":30", "\"earned\":30,\"spent\":31"), "spent JP must be from 0 to the JP earned"));
  CHECK(saysIn(tryWith("\"earned\":30", "\"earned\":30,\"learned\":[4]"), "is not taught by that job"));
  CHECK(saysIn(tryWith("\"equipped\":[0]", "\"equipped\":[0,0]"), "listed twice"));
  CHECK(saysIn(tryWith("\"equipped\":[0]", "\"equipped\":[3]"), "has not been learned"));
}

namespace {

struct Played {
  std::string state;
  int jpEvents = 0;
  bool mended = false;
};

Played playOut(const char* name) {
  jobtest::Copy c(name);
  c.swap("maps/duel.json", "\"id\": \"blue\", \"name\": \"Blue\", \"controller\": \"human\"", "\"id\": \"blue\", \"name\": \"Blue\", \"controller\": \"ai\"");
  skilltest::World w("duel", c.dir);
  Played out;
  std::vector<Event> events;
  int guard = 0;
  while (!w.b().over && w.b().round <= 30 && guard++ < 3000) {
    events.clear();
    aiTakeStep(w.b(), events);
    for (const Event& e : events) {
      out.jpEvents += e.type == EventType::JpGained;
      out.mended |= e.type == EventType::AbilityUsed;
    }
  }
  out.state = savetest::stateText(w.b());
  return out;
}

}  // namespace

TEST(a_whole_computer_game_with_jobs_is_the_same_every_time) {
  const Played a = playOut("jb_game_a"), b = playOut("jb_game_b");
  CHECK_STR(a.state, b.state);
  CHECK_EQ(a.jpEvents, b.jpEvents);
  CHECK_EQ(a.mended ? 1 : 0, b.mended ? 1 : 0);
  CHECK(a.jpEvents > 0);  // the blue side's units with jobs fought
}

TEST(an_enemy_with_a_job_uses_the_ability_it_learned) {
  Duel d("jb_ai");
  Battle& b = d.b();
  d.unit("weak").hp = 3;
  d.unit("foe").done = true;  // the others stay where they are, so the shaman's turn is what is judged
  d.unit("weak").done = true;
  std::vector<Event> ev;
  CHECK(b.apply(Action::endTurn(), ev));
  for (int i = 0; i < 80 && !b.over && b.activeSide == 1; ++i) aiTakeStep(b, ev);
  int used = 0;
  const int mend = d.w.rig.db.findAbility("mend");
  for (const Event& e : ev) used += e.type == EventType::AbilityUsed && e.a == d.w.id("shaman") && e.b == mend && e.c == d.w.id("weak");
  CHECK_EQ(used, 1);
  CHECK(d.unit("weak").hp > 3);
  // The same enemy without the learned ability has none to use.
  jobtest::Copy c("jb_ai_none");
  c.swap("maps/duel.json", "\"learned\": [\"mend\"], ", "");
  skilltest::World w("duel", c.dir);
  std::vector<int> abilities;
  w.b().abilitiesOf(w.unit("shaman"), abilities);
  CHECK(abilities.empty());
}

TEST(changing_the_jobs_data_makes_old_saves_of_a_jobs_game_refuse) {
  jobtest::Copy a("jb_hash_a"), b("jb_hash_b");
  b.swap("data/jobs.json", "\"cost\": 30", "\"cost\": 31");
  Database da, db2;
  std::string err, ha, hb;
  CHECK(loadGame(a.dir, da, err) && loadGame(b.dir, db2, err));
  CHECK(computeDataHash(da, "duel", ha, err) && computeDataHash(db2, "duel", hb, err));
  CHECK(ha != hb);
  b.swap("data/jobs.json", "\"cost\": 31", "\"cost\": 30");
  b.swap("data/jobs.json", "\"description\": \"Heavy armour.\"", "\"description\": \"Plate and shield.\"");
  Database dc;
  std::string hc;
  CHECK(loadGame(b.dir, dc, err) && computeDataHash(dc, "duel", hc, err));
  CHECK_STR(ha, hc);  // wording is not a rule
}
