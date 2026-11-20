// A writable copy of tests/fixtures/jobs (docs/JOBS.md) for the job tests, and a small campaign over it. Fixture: three jobs (Squire, Knight, Mage), the characters
// Aldric (Squire, 30 JP in it = job level 2), Bryn (Squire) and Cora (Mage; Squire 25 JP with Guard learned and equipped, Mage 10 JP), the maps "first"
// (Aldric, Bryn against a dummy and a grunt), "second" (adds Cora; the enemy shaman is a Mage that knows Mend) and "duel".
//   squire hp 30 str 6 def 2 mov 4 sword | knight hp 36 str 7 def 5 mov 3 sword | mage hp 22 str 3 def 1 mov 4 rod (range 1..2, might 3)
//   jobs:  squire teaches guard (support, 30 JP), shove (action ability, 40), light_step (movement, 25); levels 2 / 3 / 4 at 20 / 60 / 120 JP
//          knight (needs squire level 2) teaches iron_will (reaction, 60); mage (needs squire level 2, levels at 30 / 90) teaches mend (action ability, 50), focus (support, 40)
//   rules: 10 JP for taking part in a fight, 20 more for a defeat, one slot of each kind, changeIn camp+prep
#pragma once
#include <string>

#include "core/campaign/campaign_save.h"
#include "core/campaign/playthrough.h"
#include "core/jobs/job_camp.h"
#include "core/jobs/job_rules.h"
#include "core/script/formulas.h"
#include "srpg_skills_fixture.h"

namespace jobtest {

using namespace ote;

struct Copy : skilltest::Copy {
  explicit Copy(const std::string& name) : skilltest::Copy(name, "jobs") {
    static const char* const extra[] = {"data/jobs.json", "data/campaign.json", "maps/first.json", "maps/second.json"};
    for (const char* f : extra) {
      std::string text;
      if (readTextFile(ote_test::sourceDir() + "/tests/fixtures/jobs/" + f, text)) writeTextFile(dir + "/" + f, text);
    }
  }
};

inline std::string fixtureDir() { return ote_test::sourceDir() + "/tests/fixtures/jobs"; }

// The loaded game with a playthrough at chapter 1 and the helpers the camp tests share.
struct Game {
  Database db;
  Formulas formulas;
  Playthrough pt;
  Battle battle;
  std::vector<Event> events;
  std::string dir;

  bool load(const std::string& folder) {
    dir = folder;
    std::string err;
    bool ok = loadGame(folder, db, err) && formulas.init(db, err) && pt.begin(db);
    if (!ok) std::printf("  %s\n", err.c_str());
    return ok;
  }
  int job(const char* id) const { return db.findJob(id); }
  int entry(const char* id) const {
    for (const JobLearnDef& e : db.jobEntries)
      if (e.refId == id) return e.flat;
    return -1;
  }
  RosterUnit* rosterUnit(const char* id) {
    for (RosterUnit& r : pt.roster)
      if (r.id == id) return &r;
    return nullptr;
  }
  bool startChapter(uint32_t seed = 3) {
    if (pt.phase == Phase::Before) {
      std::vector<SceneLine> lines;
      if (!runScene(db, pt, SceneKind::Before, lines)) return false;
    }
    std::string err;
    events.clear();
    battle = Battle();
    if (!startChapterBattle(db, pt, formulas, seed, battle, events, err)) {
      std::printf("  startChapterBattle: %s\n", err.c_str());
      return false;
    }
    return true;
  }
  Unit& unit(const char* id) { return battle.units[(size_t)battle.unitByStoryId(id)]; }
  // The battle is won as it stands: the roster takes the units over, then the after scene runs and the next chapter is due.
  bool winAndFinish() {
    battle.over = true;
    battle.winnerTeam = battle.humanTeam();
    if (finishChapterBattle(db, pt, battle) != ChapterResult::Won) return false;
    std::vector<SceneLine> lines;
    return runScene(db, pt, SceneKind::After, lines);
  }
};

}  // namespace jobtest
