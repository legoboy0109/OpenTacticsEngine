// docs/JOBS.md: data/jobs.json, game.json "rules.jobs" and the job keys of characters and map units: what loads, what the rules over a job state say (job levels,
// unlocking, learning, equipping, slots) and the loader's refusals (each names the file, the entry and the key). Fixture tests/fixtures/jobs (see jobs_fixture.h).
#include "jobs_fixture.h"

using namespace ote;
using skilltest::saysIn;

namespace {

struct Fixture {
  Database db;
  explicit Fixture() {
    std::string err;
    const bool ok = loadGame(jobtest::fixtureDir(), db, err);
    if (!ok) std::printf("  %s\n", err.c_str());
    CHECK(ok);
  }
  int job(const char* id) const { return db.findJob(id); }
  int entry(const char* id) const {
    for (const JobLearnDef& e : db.jobEntries)
      if (e.refId == id) return e.flat;
    return -1;
  }
};

// A JobState that uses `job`, with the JP given per job id.
JobState stateOf(const Fixture& f, const char* job, std::initializer_list<std::pair<const char*, int>> earned = {}) {
  JobState s;
  s.job = f.job(job);
  for (const auto& e : earned) s.at(f.job(e.first)).earned = e.second;
  return s;
}

std::string refusal(const char* name, const char* file, const char* from, const char* to) {
  jobtest::Copy c(name);
  c.swap(file, from, to);
  return c.gameError();
}

// The same for a map: the game loads, the map is what is refused.
std::string mapRefusal(const char* name, const char* map, const char* from, const char* to) {
  jobtest::Copy c(name);
  c.swap(std::string("maps/") + map + ".json", from, to);
  return c.mapError(map);
}

}  // namespace

TEST(the_job_fixture_loads_with_its_jobs_entries_rules_and_characters) {
  Fixture f;
  const Database& db = f.db;
  CHECK(db.jobsOn());
  CHECK_EQ((int)db.jobs.size(), 3);
  CHECK_EQ((int)db.jobEntries.size(), 6);
  CHECK_STR(db.jobs[(size_t)f.job("knight")].name, "Knight");
  CHECK_EQ(db.jobs[(size_t)f.job("knight")].unitType, db.findUnitType("knight"));
  CHECK_EQ((int)db.jobs[(size_t)f.job("knight")].needs.size(), 1);
  CHECK_EQ(db.jobs[(size_t)f.job("knight")].needs[0].job, f.job("squire"));
  CHECK_EQ(db.jobEntries[(size_t)f.entry("shove")].ability ? 1 : 0, 1);
  CHECK(db.jobEntries[(size_t)f.entry("guard")].slot == JobSlot::Support);
  CHECK_EQ(db.jobEntries[(size_t)f.entry("guard")].cost, 30);
  CHECK_EQ(db.rules.jobs.jpPerAction, 10);
  CHECK_EQ(db.rules.jobs.jpPerDefeat, 20);
  CHECK(db.rules.jobs.changeInPrep);
  CHECK_EQ(db.rules.jobs.slots[(int)JobSlot::Reaction], 1);
  // A character with a job plays as the job's unit type; its starting progress is what the file says.
  const CharacterDef& cora = db.characters[(size_t)db.findCharacter("cora")];
  CHECK_EQ(cora.jobs.job, f.job("mage"));
  CHECK_EQ(cora.type, db.findUnitType("mage"));
  CHECK_EQ(cora.jobs.find(f.job("squire"))->earned, 25);
  CHECK_EQ((int)cora.jobs.equipped.size(), 1);
  CHECK_EQ(db.characters[(size_t)db.findCharacter("aldric")].jobs.find(f.job("squire"))->earned, 30);
}

TEST(a_game_without_jobs_json_has_no_jobs_and_a_unit_without_a_job_does_not_use_them) {
  Database plain;
  std::string err;
  CHECK(loadGame(ote_test::sourceDir() + "/tests/fixtures/unit-ranks", plain, err));
  CHECK(!plain.jobsOn());
  CHECK(plain.jobs.empty() && plain.jobEntries.empty());
  jobtest::Copy c("jd_empty");  // an empty list is the same game as no file
  c.put("data/jobs.json", "{ \"jobs\": [] }");
  c.swap("game.json", "\"jobs\": {", "\"ignored\": {");
  c.swap("data/characters.json", ", \"job\": \"squire\", \"jobs\": { \"squire\": { \"jp\": 30 } }", "");
  c.swap("data/characters.json", ", \"job\": \"squire\" }", " }");
  c.swap("data/characters.json", ",\n  { \"id\": \"cora\", \"name\": \"Cora\", \"type\": \"mage\", \"level\": 1, \"job\": \"mage\",\n    \"jobs\": { \"squire\": { \"jp\": 25, \"learned\": [\"guard\"] }, \"mage\": { \"jp\": 10 } }, \"equipped\": [\"guard\"] }", "");
  skilltest::World w("first", c.dir, false);
  CHECK(!w.db().jobsOn());
}

TEST(job_levels_come_from_the_total_jp_of_the_job_or_the_game_table) {
  Fixture f;
  const Database& db = f.db;
  CHECK_EQ(jobLevelAt(db, f.job("squire"), 0), 1);
  CHECK_EQ(jobLevelAt(db, f.job("squire"), 19), 1);
  CHECK_EQ(jobLevelAt(db, f.job("squire"), 20), 2);
  CHECK_EQ(jobLevelAt(db, f.job("squire"), 60), 3);
  CHECK_EQ(jobLevelAt(db, f.job("squire"), 5000), 4);  // the table's end is the top level
  CHECK_EQ(jobMaxLevel(db, f.job("squire")), 4);
  CHECK_EQ(jobLevelAt(db, f.job("mage"), 29), 1);  // the mage's own table [30, 90] replaces the game's
  CHECK_EQ(jobLevelAt(db, f.job("mage"), 30), 2);
  CHECK_EQ(jobMaxLevel(db, f.job("mage")), 3);
  CHECK_EQ(jobNextLevelAt(db, f.job("squire"), 25), 60);
  CHECK_EQ(jobNextLevelAt(db, f.job("squire"), 500), -1);
}

TEST(a_job_is_unlocked_by_the_levels_it_needs_and_the_reason_is_in_words) {
  Fixture f;
  JobState s = stateOf(f, "squire", {{"squire", 19}});
  std::string why;
  CHECK(jobUnlocked(f.db, s, f.job("squire"), &why));  // no requirements
  CHECK(!jobUnlocked(f.db, s, f.job("knight"), &why));
  CHECK_STR(why, "needs Squire level 2");
  s.at(f.job("squire")).earned = 20;
  CHECK(jobUnlocked(f.db, s, f.job("knight"), &why));
  CHECK(jobUnlocked(f.db, s, f.job("mage"), &why));
  std::string refused;
  JobState fresh = stateOf(f, "squire");
  CHECK(!jobSwitch(f.db, fresh, f.job("mage"), refused));
  CHECK_STR(refused, "needs Squire level 2");
  CHECK_EQ(fresh.job, f.job("squire"));  // a refusal changes nothing
}

TEST(learning_spends_the_jp_of_the_entrys_own_job_and_refuses_the_rest) {
  Fixture f;
  JobState s = stateOf(f, "squire", {{"squire", 50}});
  std::string why;
  CHECK(jobLearn(f.db, s, f.entry("guard"), why));
  CHECK_EQ(jobUnspent(s, f.job("squire")), 20);
  CHECK(jobHasLearned(s, f.entry("guard")));
  CHECK(!jobLearn(f.db, s, f.entry("guard"), why));
  CHECK_STR(why, "already learned");
  CHECK(!jobLearn(f.db, s, f.entry("shove"), why));  // 40 JP, 20 left
  CHECK(saysIn(why, "costs 40 JP, 20 left in Squire"));
  CHECK(jobLearn(f.db, s, f.entry("light_step"), why) == false);  // 25 JP, 20 left
  s.at(f.job("squire")).earned = 100;
  CHECK(jobLearn(f.db, s, f.entry("light_step"), why));
  CHECK_EQ(jobUnspent(s, f.job("squire")), 45);
  // A locked job teaches nothing, however much JP is lying around.
  JobState locked = stateOf(f, "squire", {{"squire", 10}, {"knight", 500}});
  CHECK(!jobLearn(f.db, locked, f.entry("iron_will"), why));
  CHECK(saysIn(why, "Knight is locked: needs Squire level 2"));
}

TEST(equipping_needs_a_learned_entry_of_another_job_and_a_free_slot) {
  Fixture f;
  JobState s = stateOf(f, "knight", {{"squire", 200}, {"mage", 200}});
  std::string why;
  CHECK(!jobEquip(f.db, s, f.entry("guard"), why));
  CHECK_STR(why, "not learned yet");
  CHECK(jobLearn(f.db, s, f.entry("guard"), why));
  CHECK(jobLearn(f.db, s, f.entry("focus"), why));
  CHECK(jobLearn(f.db, s, f.entry("mend"), why));
  CHECK(jobEquip(f.db, s, f.entry("guard"), why));
  CHECK(!jobEquip(f.db, s, f.entry("guard"), why));
  CHECK_STR(why, "already equipped");
  CHECK(!jobEquip(f.db, s, f.entry("focus"), why));  // both are support entries and there is one support slot
  CHECK(saysIn(why, "the support slots are full (1)"));
  CHECK(jobEquip(f.db, s, f.entry("mend"), why));    // an action entry: its own slot kind
  CHECK(jobUnequip(f.db, s, f.entry("guard"), why));
  CHECK(jobEquip(f.db, s, f.entry("focus"), why));
  CHECK_EQ((int)s.equipped.size(), 2);
  // Taking the job that teaches an equipped entry makes it the job's own: it leaves the equipped list.
  CHECK(jobSwitch(f.db, s, f.job("mage"), why));
  CHECK(s.equipped.empty());  // Mend and Focus are the Mage's own now
  CHECK(!jobEquip(f.db, s, f.entry("focus"), why));  // and cannot be equipped while the job is the current one
  CHECK(saysIn(why, "belongs to the current job"));
}

TEST(the_active_entries_are_the_current_jobs_learned_ones_then_the_equipped) {
  Fixture f;
  JobState s = stateOf(f, "mage", {{"squire", 100}, {"mage", 100}});
  std::string why;
  CHECK(jobLearn(f.db, s, f.entry("guard"), why));
  CHECK(jobLearn(f.db, s, f.entry("mend"), why));
  CHECK(jobEquip(f.db, s, f.entry("guard"), why));
  std::vector<int> active;
  jobActiveEntries(f.db, s, active);
  CHECK_EQ((int)active.size(), 2);
  CHECK_EQ(active[0], f.entry("mend"));
  CHECK_EQ(active[1], f.entry("guard"));
  JobState none;
  jobActiveEntries(f.db, none, active);
  CHECK(active.empty());
}

TEST(a_state_that_does_not_fit_the_game_is_named_in_words) {
  Fixture f;
  JobState s = stateOf(f, "squire", {{"squire", 10}});
  CHECK_STR(jobStateProblem(f.db, s), "");
  s.at(f.job("squire")).spent = 11;
  CHECK(saysIn(jobStateProblem(f.db, s), "spent JP must be from 0 to the JP earned"));
  s.at(f.job("squire")).spent = 0;
  s.at(f.job("squire")).learned.push_back(f.entry("mend"));
  CHECK(saysIn(jobStateProblem(f.db, s), "is not taught by that job"));
  s.at(f.job("squire")).learned.clear();
  s.equipped.push_back(f.entry("guard"));
  CHECK(saysIn(jobStateProblem(f.db, s), "has not been learned"));
  s.job = 7;
  CHECK(saysIn(jobStateProblem(f.db, s), "is not a job of this game"));
}

TEST(the_loader_names_file_entry_and_key_for_every_job_refusal) {
  CHECK(saysIn(refusal("jr1", "data/jobs.json", "\"unitType\": \"knight\"", "\"unitType\": \"ghost\""), "job \"knight\": \"unitType\" names unknown unit type \"ghost\" (data/units.json)"));
  CHECK(saysIn(refusal("jr2", "data/jobs.json", "\"skill\": \"guard\"", "\"skill\": \"nope\""), "\"learn\" entry 1: \"skill\" names unknown skill \"nope\" (data/skills.json)"));
  CHECK(saysIn(refusal("jr3", "data/jobs.json", "\"ability\": \"mend\"", "\"ability\": \"nope\""), "\"ability\" names unknown ability \"nope\" (data/abilities.json)"));
  CHECK(saysIn(refusal("jr4", "data/jobs.json", "\"id\": \"knight\"", "\"id\": \"squire\""), "job \"squire\": duplicate id"));
  CHECK(saysIn(refusal("jr5", "data/jobs.json", "\"skill\": \"light_step\"", "\"skill\": \"guard\""), "\"guard\" is listed twice (also by job \"squire\")"));
  CHECK(saysIn(refusal("jr6", "data/jobs.json", "\"requires\": [ { \"job\": \"squire\", \"level\": 2 } ],\n    \"learn\": [ { \"skill\": \"iron_will\"", "\"requires\": [ { \"job\": \"nobody\", \"level\": 2 } ],\n    \"learn\": [ { \"skill\": \"iron_will\""), "names unknown job \"nobody\""));
  CHECK(saysIn(refusal("jr7", "data/jobs.json", "\"unitType\": \"squire\",", "\"unitType\": \"squire\", \"requires\": [ { \"job\": \"knight\", \"level\": 1 } ],"), "\"requires\" goes round in a circle"));
  CHECK(saysIn(refusal("jr8", "data/jobs.json", "\"level\": 2 } ],\n    \"learn\": [ { \"skill\": \"iron_will\"", "\"level\": 9 } ],\n    \"learn\": [ { \"skill\": \"iron_will\""), "only has 4 job levels"));
  CHECK(saysIn(refusal("jr9", "data/jobs.json", "\"cost\": 30", "\"cost\": -3"), "\"cost\" must be a whole number from 0 to 99999"));
  CHECK(saysIn(refusal("jr10", "data/jobs.json", "\"cost\": 30, \"slot\": \"support\"", "\"cost\": 30, \"slot\": \"nowhere\""), "\"slot\" must be \"action\""));
  CHECK(saysIn(refusal("jr11", "data/jobs.json", "\"jpLevels\": [30, 90]", "\"jpLevels\": [30, 20]"), "\"jpLevels\" entry 2 must be higher than the one before it"));
  CHECK(saysIn(refusal("jr12", "data/jobs.json", "{ \"skill\": \"guard\", \"cost\": 30, \"slot\": \"support\" }", "{ \"skill\": \"guard\", \"ability\": \"shove\", \"cost\": 30, \"slot\": \"support\" }"), "give \"skill\""));
  CHECK(saysIn(refusal("jr13", "game.json", "\"action\": 1", "\"action\": -1"), "rules.jobs, \"slots\": \"action\" must be a whole number from 0 to 8"));
  CHECK(saysIn(refusal("jr14", "game.json", "\"changeIn\": \"camp+prep\"", "\"changeIn\": \"anywhere\""), "\"changeIn\" must be \"camp\""));
  CHECK(saysIn(refusal("jr15", "game.json", "\"perAction\": 10", "\"perAction\": 10, \"share\": 5"), "unknown key \"share\""));
  CHECK(saysIn(refusal("jr16", "game.json", "\"jpLevels\": [20, 60, 120]", "\"jpLevels\": [20, 20]"), "rules.jobs: \"jpLevels\" entry 2 must be higher"));
  // The id of a skill that is also the id of a taught ability could not be told apart in a "learned" list.
  jobtest::Copy both("jr17");
  const std::string block = "\"blocks\": [ { \"when\": \"always\", \"do\": [ { \"stats\": { \"mov\": 1 } } ] } ]";
  both.put("data/skills.json", "{ \"skills\": [ { \"id\": \"guard\", " + block + " }, { \"id\": \"light_step\", " + block + " }, { \"id\": \"iron_will\", " + block + " }, { \"id\": \"focus\", " + block + " }, { \"id\": \"shove\", " + block + " } ] }");
  both.swap("data/jobs.json", "{ \"ability\": \"shove\", \"cost\": 40, \"slot\": \"action\" }", "{ \"ability\": \"shove\", \"cost\": 40, \"slot\": \"action\" }, { \"skill\": \"shove\", \"cost\": 40, \"slot\": \"action\" }");
  CHECK(saysIn(both.gameError(), "\"shove\" is taught as a skill and as an ability"));
}

TEST(a_character_or_unit_with_job_keys_is_checked_against_its_jobs) {
  CHECK(saysIn(refusal("jc1", "data/characters.json", "\"job\": \"mage\",", "\"job\": \"jester\","), "character \"cora\": \"job\" names unknown job \"jester\""));
  CHECK(saysIn(refusal("jc2", "data/characters.json", "\"learned\": [\"guard\"]", "\"learned\": [\"mend\"]"), "\"jobs\" \"squire\": \"learned\" names \"mend\", which job \"squire\" does not teach"));
  CHECK(saysIn(refusal("jc3", "data/characters.json", "\"learned\": [\"guard\"]", "\"learned\": [\"guard\", \"guard\"]"), "lists \"guard\" twice"));
  jobtest::Copy own("jc4");  // Focus is the Mage's own entry, and Cora is a Mage
  own.swap("data/characters.json", "\"mage\": { \"jp\": 10 } }, \"equipped\": [\"guard\"]", "\"mage\": { \"jp\": 10, \"learned\": [\"focus\"] } }, \"equipped\": [\"focus\"]");
  CHECK(saysIn(own.gameError(), "equipped \"focus\" belongs to the current job"));
  CHECK(saysIn(refusal("jc5", "data/characters.json", "\"equipped\": [\"guard\"]", "\"equipped\": [\"iron_will\"]"), "equipped \"iron_will\" has not been learned"));
  CHECK(saysIn(refusal("jc6", "data/characters.json", "\"equipped\": [\"guard\"]", "\"equipped\": [\"nothing\"]"), "\"equipped\" names \"nothing\", which no job teaches"));
  CHECK(saysIn(refusal("jc7", "game.json", "\"support\": 1", "\"support\": 0"), "more equipped entries than support slots (0)"));
  CHECK(saysIn(refusal("jc8", "data/characters.json", "\"id\": \"bryn\", \"name\": \"Bryn\", \"type\": \"squire\", \"level\": 1, \"job\": \"squire\"", "\"id\": \"bryn\", \"name\": \"Bryn\", \"type\": \"squire\", \"level\": 1, \"equipped\": [\"guard\"]"), "belong to a unit that has a job"));
  CHECK(saysIn(refusal("jc9", "data/characters.json", "\"jp\": 30", "\"jp\": -4"), "\"jp\" must be a whole number from 0 to"));
  // The jobs files must go together.
  jobtest::Copy none("jc10");
  none.drop("data/jobs.json");
  CHECK(saysIn(none.gameError(), "game.json: rules.jobs: needs data/jobs.json"));
  none.swap("game.json", "\"jobs\": {", "\"ignored\": {");
  CHECK(saysIn(none.gameError(), "character \"aldric\""));
  CHECK(saysIn(none.gameError(), "need jobs: data/jobs.json"));
  // Map units: the job decides the unit type; keys of a unit that has none are refused.
  CHECK(saysIn(mapRefusal("jm1", "duel", "\"job\": \"mage\", \"learned\"", "\"type\": \"grunt\", \"job\": \"mage\", \"learned\""), "the job decides the unit type"));
  CHECK(saysIn(mapRefusal("jm2", "duel", "\"job\": \"mage\", \"learned\": [\"mend\"], \"equipped\": []", "\"learned\": [\"mend\"]"), "missing required \"type\""));
  CHECK(saysIn(mapRefusal("jm3", "duel", "\"job\": \"mage\"", "\"job\": \"jester\""), "\"job\" names unknown job \"jester\""));
  CHECK(saysIn(mapRefusal("jm4", "duel", "\"learned\": [\"mend\"]", "\"learned\": [\"guard\"]"), "\"learned\" names \"guard\", which the job \"mage\" does not teach"));
  CHECK(saysIn(mapRefusal("jm5", "duel", "\"equipped\": []", "\"equipped\": [\"guard\"]"), "equipped \"guard\" has not been learned"));
}
