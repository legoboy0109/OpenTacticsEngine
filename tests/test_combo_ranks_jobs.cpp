// Levelling + promotion + merit ranks + class jobs + overlay jobs in one game (docs/JOBS.md "Class jobs and overlay jobs", docs/SRPG.md "Unit ranks" merit mode,
// "Combining levels, ranks and jobs"). Fixture tests/fixtures/combo (combo_fixture.h): overlay jobs keep the unit's class, level, stats and promotion path and
// add a bonus and learned skills; class jobs set the unit type as ever; a unit in a class job does not promote.
#include "combo_fixture.h"
#include "core/ai/ai.h"
#include "core/battle/save.h"
#include "core/campaign/campaign_save.h"
#include "core/campaign/prep.h"

using namespace ote;
using skilltest::saysIn;

namespace {

int stat(const jobtest::Game& g, const std::vector<int>& stats, const char* s) { return stats[(size_t)g.db.findStat(s)]; }

// Chapter 1 played: Bryn (scout; one hit from level 2) and Aldric (class job squire; one hit from level 2, merit 8) each strike the dummy once; the battle is won.
struct AfterChapterOne {
  combotest::Copy c;
  jobtest::Game g;
  std::string why;
  explicit AfterChapterOne(const char* name) : c(name) {
    CHECK(g.load(c.dir));
    CHECK(g.startChapter());
    g.unit("bryn").xp = 10;
    g.unit("aldric").xp = 10;
    const int sword = g.db.findWeapon("sword");
    CHECK(g.battle.apply(Action::attack(g.battle.unitByStoryId("aldric"), g.battle.unitByStoryId("foe"), sword), g.events));
    CHECK(g.battle.apply(Action::move(g.battle.unitByStoryId("bryn"), g.battle.grid.at(3, 3)), g.events));  // 3 squares: the scout's +1 move helps nothing here, but it is allowed
    CHECK(g.battle.apply(Action::attack(g.battle.unitByStoryId("bryn"), g.battle.unitByStoryId("foe"), sword), g.events));
    CHECK(g.winAndFinish());
  }
  RosterUnit& r(const char* id) { return *g.rosterUnit(id); }
  bool change(const char* who, const char* job) { return jobsChange(g.db, g.pt, who, g.job(job), why); }
};

}  // namespace

TEST(the_combo_game_loads_with_class_jobs_and_overlay_jobs_and_characters_remember_their_own_class) {
  combotest::Copy c("co_load");
  jobtest::Game g;
  CHECK(g.load(c.dir));
  const Database& db = g.db;
  CHECK(db.jobsOverlay());
  CHECK(!db.jobs[(size_t)g.job("squire")].overlay());
  CHECK(db.jobs[(size_t)g.job("scout")].overlay());
  CHECK_EQ(db.jobs[(size_t)g.job("scout")].bonus[(size_t)db.findStat("str")], 1);
  CHECK_EQ(db.jobs[(size_t)g.job("scout")].bonus[(size_t)db.findStat("mov")], 1);
  CHECK(db.rules.unitRanks.merit && db.rules.progression);
  const CharacterDef& bryn = db.characters[(size_t)db.findCharacter("bryn")];
  CHECK_EQ(bryn.type, db.findUnitType("squire"));  // an overlay job leaves the type
  CHECK_EQ(bryn.jobs.baseType, db.findUnitType("squire"));
  const CharacterDef& cora = db.characters[(size_t)db.findCharacter("cora")];
  CHECK_EQ(cora.type, db.findUnitType("mage"));
  CHECK_EQ(cora.jobs.baseType, db.findUnitType("mage"));
}

TEST(an_overlay_job_adds_its_bonus_and_skills_and_keeps_the_unit_type_and_stats) {
  combotest::Copy c("co_overlay");
  jobtest::Game g;
  CHECK(g.load(c.dir) && g.startChapter());
  Unit& bryn = g.unit("bryn");
  CHECK_EQ(bryn.type, g.db.findUnitType("squire"));
  CHECK_EQ(bryn.stats[(size_t)g.db.findStat("str")], 6);  // the unit's own stat: nothing was swapped
  CHECK_EQ(g.battle.stat(bryn, g.db.findStat("str")), 7);  // + the scout's bonus
  CHECK_EQ(g.battle.stat(bryn, g.db.findStat("mov")), 5);
  CHECK_EQ(g.battle.stat(g.unit("aldric"), g.db.findStat("str")), 6);  // a class job without a bonus adds nothing (merit 8 is below Veteran)
}

TEST(a_class_job_unit_does_not_promote_but_an_overlay_job_unit_promotes_its_own_class) {
  AfterChapterOne a("co_promo");
  RosterUnit& bryn = a.r("bryn");
  CHECK_EQ(bryn.level, 2);
  CHECK_EQ(bryn.type, a.g.db.findUnitType("captain"));  // promoted while a scout
  CHECK_EQ(bryn.jobs.job, a.g.job("scout"));
  CHECK_EQ(bryn.jobs.baseType, a.g.db.findUnitType("captain"));
  CHECK_EQ(stat(a.g, bryn.stats, "def"), 3);  // squire 2 + (captain 3 - squire 2)
  CHECK_EQ(stat(a.g, bryn.stats, "mov"), 4);
  RosterUnit& aldric = a.r("aldric");
  CHECK_EQ(aldric.level, 2);  // levelled all the same ...
  CHECK_EQ(aldric.type, a.g.db.findUnitType("squire"));  // ... but a unit in a class job stays what the job makes it
  CHECK_EQ(aldric.jobs.baseType, a.g.db.findUnitType("squire"));
}

TEST(merit_and_experience_and_jp_all_go_to_the_same_unit_and_the_roster_carries_them) {
  AfterChapterOne a("co_carry");
  RosterUnit& aldric = a.r("aldric");
  CHECK_EQ(aldric.merit, 8 + 5);  // the map gave 8, the hit 5
  CHECK_EQ(aldric.jobs.find(a.g.job("squire"))->earned, 40);
  CHECK_EQ(a.r("bryn").merit, 5);
  CHECK_EQ(a.r("bryn").jobs.find(a.g.job("scout"))->earned, 35);
}

TEST(changing_between_class_and_overlay_jobs_follows_the_base_type_and_back_is_exact) {
  AfterChapterOne a("co_change");
  RosterUnit& bryn = a.r("bryn");
  const std::vector<int> before = bryn.stats;  // a captain (promoted as a scout)
  bryn.jobs.at(a.g.job("squire")).earned = 30;     // Knight needs Squire level 2
  CHECK(a.change("bryn", "knight"));  // class job
  CHECK_EQ(bryn.type, a.g.db.findUnitType("knight"));
  CHECK_EQ(bryn.jobs.baseType, a.g.db.findUnitType("captain"));  // remembered
  CHECK_EQ(stat(a.g, bryn.stats, "def"), 3 + (5 - 3));
  CHECK_EQ(stat(a.g, bryn.stats, "mov"), 3);
  CHECK(a.change("bryn", "warden"));  // overlay: plays as the base type again
  CHECK_EQ(bryn.type, a.g.db.findUnitType("captain"));
  CHECK(bryn.stats == before);
  CHECK(a.change("bryn", "scout"));  // overlay to overlay: nothing moves
  CHECK(bryn.stats == before);
  // Aldric: a class job to an overlay job keeps squire; to another class job and back is exact.
  RosterUnit& aldric = a.r("aldric");
  const std::vector<int> own = aldric.stats;
  CHECK(a.change("aldric", "warden"));
  CHECK_EQ(aldric.type, a.g.db.findUnitType("squire"));
  CHECK(aldric.stats == own);
  CHECK(a.change("aldric", "mage"));
  CHECK_EQ(aldric.type, a.g.db.findUnitType("mage"));
  CHECK(a.change("aldric", "warden"));
  CHECK_EQ(aldric.type, a.g.db.findUnitType("squire"));
  CHECK(aldric.stats == own);
}

TEST(the_next_chapter_dresses_the_unit_from_the_roster_with_level_merit_job_and_bonus) {
  AfterChapterOne a("co_next");
  CHECK(a.change("aldric", "warden"));
  CHECK(a.g.startChapter());
  Unit& aldric = a.g.unit("aldric");
  // A warden (overlay job) at level 2: the automatic promotion that a class job held back arrives with the first turn of the chapter.
  CHECK_EQ(aldric.type, a.g.db.findUnitType("captain"));
  CHECK_EQ(aldric.jobs.baseType, a.g.db.findUnitType("captain"));
  CHECK_EQ(aldric.level, 2);
  CHECK_EQ(aldric.merit, 13);
  CHECK_EQ(a.g.battle.unitRank(aldric), 0);  // Veteran
  const int base = aldric.stats[(size_t)a.g.db.findStat("def")];
  CHECK_EQ(a.g.battle.stat(aldric, a.g.db.findStat("def")), base + 2);  // the warden's bonus
  Unit& bryn = a.g.unit("bryn");
  CHECK_EQ(bryn.type, a.g.db.findUnitType("captain"));
  CHECK_EQ(a.g.battle.stat(bryn, a.g.db.findStat("mov")), 4 + 1);
  // The red raider is a grunt with an overlay job; the shaman a mage by class job.
  CHECK_EQ(a.g.unit("raider").type, a.g.db.findUnitType("grunt"));
  CHECK_EQ(a.g.unit("raider").jobs.baseType, a.g.db.findUnitType("grunt"));
  CHECK_EQ(a.g.battle.stat(a.g.unit("raider"), a.g.db.findStat("str")), 3 + 1);
}

TEST(the_combo_roster_and_battle_go_through_saves_and_inconsistent_ones_are_refused) {
  AfterChapterOne a("co_save");
  CHECK(a.change("aldric", "warden"));
  Json j;
  savePlaythrough(a.g.db, a.g.pt, j);
  const std::string text = j.dump(0);
  CHECK(text.find("\"merit\":13") != std::string::npos);
  CHECK(text.find("\"baseType\"") != std::string::npos);
  Playthrough back;
  std::string err;
  CHECK(restorePlaythrough(a.g.db, j, back, err));
  if (!err.empty()) std::printf("  %s\n", err.c_str());
  Json again;
  savePlaythrough(a.g.db, back, again);
  CHECK_STR(again.dump(0), text);
  auto tryRoster = [&](const char* from, const char* to) -> std::string {
    std::string t = text;
    const size_t at = t.find(from);
    if (at == std::string::npos) return std::string("(missing) ") + from;
    t.replace(at, std::string(from).size(), to);
    Json bad;
    std::string perr;
    if (!Json::parse(t, bad, perr)) return "(parse) " + perr;
    Playthrough out;
    return restorePlaythrough(a.g.db, bad, out, perr) ? "" : perr;
  };
  CHECK(saysIn(tryRoster("\"merit\":13", "\"merit\":99"), "merit: value 99 is outside 0..30"));
  CHECK(saysIn(tryRoster("\"merit\":13", "\"zz\":13"), "merit: missing"));
  CHECK(saysIn(tryRoster("\"baseType\":1", "\"baseType\":2"), "must have the unit type \"knight\" (it has \"captain\")"));  // Bryn: a captain scout cannot claim knight as its own class
  // A battle in progress of the second chapter.
  CHECK(a.g.startChapter());
  Json b;
  saveBattleState(a.g.battle, b);
  const std::string btext = b.dump(0);
  skilltest::World fresh("second", a.c.dir, false);
  CHECK(restoreBattleState(fresh.b(), b, err));
  if (!err.empty()) std::printf("  %s\n", err.c_str());
  // (the restored battle is not the campaign's, but the same map: the units read back whole)
  CHECK(fresh.unit("raider").jobs == a.g.unit("raider").jobs);
  CHECK(btext.find("\"baseType\"") != std::string::npos);
}

namespace {

struct Played {
  std::string state;
  int jp = 0, rankUps = 0, promotions = 0, levelUps = 0;
};

Played playOut(const char* name) {
  combotest::Copy c(name);
  c.swap("maps/duel.json", "\"id\": \"blue\", \"name\": \"Blue\", \"controller\": \"human\"", "\"id\": \"blue\", \"name\": \"Blue\", \"controller\": \"ai\"");
  skilltest::World w("duel", c.dir);
  Played out;
  std::vector<Event> events;
  int guard = 0;
  while (!w.b().over && w.b().round <= 40 && guard++ < 4000) {
    events.clear();
    aiTakeStep(w.b(), events);
    for (const Event& e : events) {
      out.jp += e.type == EventType::JpGained;
      out.rankUps += e.type == EventType::UnitRankUp;
      out.promotions += e.type == EventType::UnitPromoted;
      out.levelUps += e.type == EventType::LevelUp;
    }
  }
  out.state = savetest::stateText(w.b());
  return out;
}

}  // namespace

TEST(a_whole_computer_game_with_levels_ranks_and_jobs_finishes_the_same_every_time) {
  const Played a = playOut("co_ai_a"), b = playOut("co_ai_b");
  CHECK_STR(a.state, b.state);
  CHECK_EQ(a.jp, b.jp);
  CHECK_EQ(a.rankUps, b.rankUps);
  CHECK_EQ(a.promotions, b.promotions);
  CHECK(a.jp > 0);
  CHECK(a.rankUps > 0);
  CHECK(a.levelUps > 0);
}

TEST(a_game_where_every_job_is_an_overlay_loads_and_plays_with_promotion) {
  combotest::Copy c("co_alloverlay");
  c.swap("data/jobs.json", ", \"unitType\": \"squire\"", "");
  c.swap("data/jobs.json", ", \"unitType\": \"knight\"", "");
  c.swap("data/jobs.json", ", \"unitType\": \"mage\"", "");
  // A map unit with a job and no type has no class to keep: refused, naming the file, the unit and the job.
  CHECK(saysIn(c.mapError("second"), "keeps the unit's own class, so the unit needs a \"type\""));
  c.swap("maps/second.json", "{ \"id\": \"shaman\"", "{ \"type\": \"mage\", \"id\": \"shaman\"");
  c.swap("maps/duel.json", "{ \"id\": \"shaman\"", "{ \"type\": \"mage\", \"id\": \"shaman\"");
  CHECK_STR(c.mapError("second"), "");
  jobtest::Game g;
  CHECK(g.load(c.dir));
  for (const JobDef& j : g.db.jobs) CHECK(j.overlay());
  CHECK(g.startChapter());
  // Aldric's job is an overlay now, so level 2 promotes him.
  g.unit("aldric").xp = 10;
  const int sword = g.db.findWeapon("sword");
  CHECK(g.battle.apply(Action::attack(g.battle.unitByStoryId("aldric"), g.battle.unitByStoryId("foe"), sword), g.events));
  CHECK_EQ(g.unit("aldric").type, g.db.findUnitType("captain"));
  CHECK_EQ(g.unit("aldric").jobs.baseType, g.db.findUnitType("captain"));
  CHECK(g.winAndFinish());
  std::string why;
  CHECK(jobsChange(g.db, g.pt, "aldric", g.job("knight"), why));  // an overlay job now: no stats move
  CHECK_EQ(g.rosterUnit("aldric")->type, g.db.findUnitType("captain"));
  // The preparation screen's "cannot stand" test judges the type the unit plays as (the captain), not a job's.
  const char* plain = "\"sides\": [ { \"id\": \"blue\", \"name\": \"Blue\", \"controller\": \"human\" },";
  const char* withPrep = "\"prep\": { \"slots\": [ { \"x\": 1, \"y\": 1 }, { \"x\": 1, \"y\": 2 }, { \"x\": 1, \"y\": 3 } ], \"limit\": 3 }, \"sides\": [ { \"id\": \"blue\", \"name\": \"Blue\", \"controller\": \"human\" },";
  c.swap("maps/second.json", plain, withPrep);
  jobtest::Game h;
  CHECK(h.load(c.dir));
  h.pt = g.pt;
  MapDef map;
  std::string err;
  CHECK(loadMap(h.db, "second", map, err));
  prepOpen(h.db, h.pt, map, true);
  CHECK(jobsChangeInPrep(h.db, h.pt, map, "aldric", h.db.findJob("mage"), why));
  CHECK_EQ(h.rosterUnit("aldric")->type, h.db.findUnitType("captain"));
}

TEST(the_loader_refuses_wrong_overlay_job_data_naming_the_file_the_entry_and_the_key) {
  auto err = [](const char* name, const char* file, const char* from, const char* to) {
    combotest::Copy c(name);
    c.swap(file, from, to);
    return c.gameError();
  };
  CHECK_STR(err("co0", "data/jobs.json", "\"bonus\": { \"def\": 2 }", "\"bonus\": { \"def\": 2 }"), "");
  CHECK(saysIn(err("co1", "data/jobs.json", "\"bonus\": { \"def\": 2 }", "\"bonus\": { \"luck\": 2 }"), "data/jobs.json: job \"warden\": \"bonus\" names unknown stat \"luck\""));
  CHECK(saysIn(err("co2", "data/jobs.json", "\"description\": \"Heavy armour.\", \"unitType\": \"knight\"", "\"description\": \"Heavy armour.\", \"unitType\": \"nosuch\""),
               "job \"knight\": \"unitType\" names unknown unit type \"nosuch\""));
  CHECK(saysIn(err("co3", "data/jobs.json", "\"bonus\": { \"def\": 2 }", "\"bonus\": { \"def\": 2 }, \"colour\": 1"), "unknown key \"colour\""));
  // A class job keeps refusing a "type" on the map unit; an overlay job needs one.
  combotest::Copy c("co4");
  c.swap("maps/duel.json", "{ \"id\": \"shaman\"", "{ \"type\": \"mage\", \"id\": \"shaman\"");
  CHECK(saysIn(c.mapError("duel"), "\"type\" and a class job together"));
  combotest::Copy d("co5");
  d.swap("maps/duel.json", "{ \"type\": \"grunt\", \"job\": \"scout\"", "{ \"job\": \"scout\"");
  CHECK(saysIn(d.mapError("duel"), "job \"scout\" keeps the unit's own class, so the unit needs a \"type\""));
  // Saves of a game without overlay jobs carry no baseType, and one that has it is refused there.
  jobtest::Copy plain("co_plain");
  jobtest::Game g;
  CHECK(g.load(plain.dir));
  CHECK(!g.db.jobsOverlay());
  JobState s;
  s.job = 0;
  s.baseType = 0;
  CHECK(saysIn(jobStateProblem(g.db, s), "\"baseType\" belongs to games with overlay jobs"));
}
