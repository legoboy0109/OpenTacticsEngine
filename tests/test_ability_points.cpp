// docs/JOBS.md "Learning from equipment (ability points)", "Sharing JP with allies", "Unlocking by mastered entries": what items teach, the points a unit
// earns in it, mastering, the JP shared with allies, job requirements on mastered entries, "spendJp": false, saves. Fixture: teaching_fixture.h.
#include <algorithm>

#include "core/battle/save.h"
#include "core/jobs/mastery.h"
#include "teaching_fixture.h"

using namespace ote;
using skilltest::saysIn;
using teachtest::Duel;

TEST(items_teach_entries_once_each_and_link_to_the_job_that_teaches_them_too) {
  Duel d("teachpts_load", false);
  const Database& db = d.db();
  CHECK(db.teachingOn());
  CHECK_EQ((int)db.teachEntries.size(), 4);
  CHECK_EQ(d.teach("brave"), 0);  // first named, first listed
  CHECK_EQ(db.teachEntries[(size_t)d.teach("brave")].jobEntry, -1);
  CHECK_EQ(db.teachEntries[(size_t)d.teach("guard")].jobEntry, d.entry("guard"));
  CHECK_EQ(db.teachEntries[(size_t)d.teach("mend")].jobEntry, d.entry("mend"));
  CHECK(db.teachEntries[(size_t)d.teach("mend")].ability);
  CHECK_EQ(db.rules.abilityPoints.perAction, 5);
  CHECK_STR(db.rules.abilityPoints.name, "AP");
}

TEST(the_loader_refuses_teaching_that_does_not_fit_in_plain_words) {
  using teachtest::Copy;
  using teachtest::refusal;
  CHECK(saysIn(refusal("teachpts_r1", [](Copy& c) { c.swap("data/items.json", R"("points": 20 })", R"("points": 0 })"); }),
               "\"teaches\" entry 1: \"points\" must be a whole number from 1 to 9999"));
  CHECK(saysIn(refusal("teachpts_r2", [](Copy& c) { c.swap("data/items.json", R"("skill": "brave")", R"("skill": "bold")"); }),
               "\"skill\" names unknown skill \"bold\""));
  CHECK(saysIn(refusal("teachpts_r3", [](Copy& c) { c.swap("data/items.json", R"("ability": "mend", "points": 15)", R"("ability": "mend", "skill": "guard", "points": 15)"); }),
               "give \"skill\" (a skill id) or \"ability\" (a menu ability id), one of them"));
  CHECK(saysIn(refusal("teachpts_r4", [](Copy& c) { c.swap("data/items.json", R"("ability": "mend", "points": 15)", R"("skill": "brave", "points": 25)"); }),
               "\"brave\" costs 25 points here but 20 on item \"charm\": an entry has one cost"));
  CHECK(saysIn(refusal("teachpts_r5", [](Copy& c) { c.swap("data/items.json", R"("kind": "accessory", "teaches": [ { "ability")", R"("kind": "usable", "heal": 5, "teaches": [ { "ability")"); }),
               "\"teaches\" belongs to a weapon or an accessory"));
  CHECK(saysIn(refusal("teachpts_r6", [](Copy& c) { c.put("data/items.json", R"({ "items": [ { "id": "ring", "name": "Ring", "kind": "accessory", "bonus": { "str": 1 } } ] })"); }),
               "rules.abilityPoints: no item \"teaches\" anything"));
  CHECK(saysIn(refusal("teachpts_r7", [](Copy& c) { c.swap("game.json", R"("name": "AP", "perBattle": 10)", R"("name": "AbilityPts", "perBattle": 10)"); }),
               "\"name\" must be 1 to 8 characters"));
  // Requirements on mastered entries.
  CHECK(saysIn(refusal("teachpts_r8", [](Copy& c) { c.swap("data/jobs.json", R"([ { "job": "squire", "level": 2 } ],
    "learn": [ { "skill": "iron_will")", R"([ { "job": "squire", "mastered": 4 } ],
    "learn": [ { "skill": "iron_will")"); }),
               "\"mastered\" must be a whole number from 1 to 3 (job \"squire\" teaches 3 entries)"));
  CHECK(saysIn(refusal("teachpts_r9", [](Copy& c) { c.swap("data/jobs.json", R"([ { "job": "squire", "level": 2 } ],
    "learn": [ { "skill": "iron_will")", R"([ { "mastered": "bold" } ],
    "learn": [ { "skill": "iron_will")"); }),
               "\"mastered\" names \"bold\", which no job or item teaches"));
  CHECK(saysIn(refusal("teachpts_r10", [](Copy& c) { c.swap("data/jobs.json", R"([ { "job": "squire", "level": 2 } ],
    "learn": [ { "skill": "iron_will")", R"([ { "job": "squire", "level": 2, "mastered": 1 } ],
    "learn": [ { "skill": "iron_will")"); }),
               "give \"level\" or \"mastered\" with \"job\""));
  CHECK(saysIn(refusal("teachpts_r11", [](Copy& c) { c.swap("data/jobs.json", R"([ { "job": "squire", "level": 2 } ],
    "learn": [ { "skill": "iron_will")", R"([ { "mastered": "iron_will" } ],
    "learn": [ { "skill": "iron_will")"); }),
               "a job cannot require what it teaches itself"));
  // "spendJp": false: no prices, and every entry must come from an item.
  CHECK(saysIn(refusal("teachpts_r12", [](Copy& c) { c.swap("game.json", R"("changeIn": "camp+prep" },)", R"("changeIn": "camp+prep", "spendJp": false },)"); }),
               "\"cost\" is not used: rules.jobs \"spendJp\" is false (entries are learned from equipment)"));
  CHECK(saysIn(refusal("teachpts_r13",
                       [](Copy& c) {
                         c.swap("game.json", R"("changeIn": "camp+prep" },)", R"("changeIn": "camp+prep", "spendJp": false },)");
                         std::string t = c.read("data/jobs.json");
                         for (const char* cost : {", \"cost\": 30", ", \"cost\": 40", ", \"cost\": 25", ", \"cost\": 60", ", \"cost\": 50"})
                           for (size_t at; (at = t.find(cost)) != std::string::npos;) t.erase(at, std::string(cost).size());
                         c.put("data/jobs.json", t);
                       }),
               "\"shove\" is learned only from equipment (\"spendJp\": false), but no item teaches it"));
}

TEST(points_go_to_what_the_equipped_items_teach_and_only_what_the_current_job_may_use) {
  Duel d("teachpts_earn");
  d.give("aldric", "charm");
  d.give("aldric", "tome");
  const Unit& aldric = d.unit("aldric");
  std::vector<int> now;
  d.b().teachFromItems(aldric, now);
  CHECK_EQ((int)now.size(), 2);  // brave (no job) and guard (Squire's, and he is a Squire); focus and mend are the Mage's
  CHECK_STR(d.w.skillIds(aldric), "brave guard");  // usable while the charm is carried
  CHECK(d.strike("aldric", "foe"));
  const std::vector<Event> pts = d.of(EventType::PointsGained);
  CHECK_EQ((int)pts.size(), 2);
  CHECK_EQ(pts[0].c, d.teach("brave"));
  CHECK_EQ(pts[0].b, 5);
  CHECK_EQ(pts[0].d, 5);
  CHECK_EQ(pts[1].c, d.teach("guard"));
  CHECK(!(pts[1].flags & kMastered));
  CHECK_EQ(teachPoints(d.unit("aldric").mastery, d.teach("guard")), 5);
  // The dummy struck back and earned experience, but an AI side learns nothing.
  for (const Event& e : pts) CHECK_EQ(e.a, d.w.id("aldric"));
}

TEST(a_full_entry_is_mastered_for_good_and_a_job_entry_is_learned_in_its_job_without_jp) {
  Duel d("teachpts_master");
  d.give("aldric", "charm");
  Unit& aldric = d.unit("aldric");
  aldric.mastery = {{d.teach("brave"), 18}, {d.teach("guard"), 28}};
  const int spentBefore = aldric.jobs.find(d.job("squire"))->spent;
  CHECK(d.strike("aldric", "foe"));
  const std::vector<Event> pts = d.of(EventType::PointsGained);
  CHECK_EQ((int)pts.size(), 2);
  CHECK((pts[0].flags & kMastered) && pts[0].d == 20);
  CHECK((pts[1].flags & kMastered) && pts[1].d == 30);
  const Unit& after = d.unit("aldric");
  CHECK(jobHasLearned(after.jobs, d.entry("guard")));  // learned in the Squire job, as if bought ...
  CHECK_EQ(after.jobs.find(d.job("squire"))->spent, spentBefore);  // ... with no JP spent
  CHECK_EQ((int)after.mastery.size(), 1);  // the job entry keeps no points; brave keeps 20/20
  CHECK_EQ(teachPoints(after.mastery, d.teach("brave")), 20);
  // Without the charm: brave stays (mastered), guard stays (learned in the job it uses now).
  d.unit("aldric").items.clear();
  CHECK_STR(d.w.skillIds(d.unit("aldric")), "guard brave");
}

TEST(a_defeat_and_a_won_battle_give_their_points) {
  Duel d("teachpts_battle");
  d.c.swap("game.json", R"("defeat": ["noUnits"])", R"("defeat": ["leaderDead"])");
  skilltest::World w("duel", d.c.dir);
  ItemStack s;
  s.item = w.db().findItem("charm");
  w.unit("aldric").items.push_back(s);
  w.unit("foe").hp = 1;
  CHECK(w.apply(Action::attack(w.id("aldric"), w.id("foe"), w.db().findWeapon("sword"))));
  CHECK(w.b().over);
  std::vector<int> amounts;  // of brave, the first entry: the defeat (3), the win (10, the defeat ended the battle), the action (5)
  for (const Event& e : w.events)
    if (e.type == EventType::PointsGained && e.c == 0) amounts.push_back(e.b);
  CHECK_EQ((int)amounts.size(), 3);
  std::sort(amounts.begin(), amounts.end());
  CHECK(amounts.size() == 3 && amounts[0] == 3 && amounts[1] == 5 && amounts[2] == 10);
  CHECK_EQ(teachPoints(w.unit("aldric").mastery, 0), 18);
}

TEST(shared_jp_goes_to_the_rest_of_the_side_in_the_earners_job) {
  Duel d("teachpts_share", false);
  d.c.swap("game.json", R"("perDefeat": 20 },)", R"("perDefeat": 20, "share": 50 },)");
  skilltest::World w("duel", d.c.dir);
  const int squire = w.db().findJob("squire");
  const int coraBefore = w.unit("cora").jobs.find(squire)->earned;
  CHECK(w.apply(Action::attack(w.id("aldric"), w.id("foe"), w.db().findWeapon("sword"))));
  int own = 0, shared = 0;
  for (const Event& e : w.events) {
    if (e.type != EventType::JpGained) continue;
    if (e.flags & kJobShared) {
      CHECK_EQ(e.a, w.id("cora"));  // the plain squire has no job; the shaman is on the other side
      CHECK_EQ(e.c, squire);
      shared += e.b;
    } else {
      own += e.b;
    }
  }
  CHECK_EQ(own, 10);
  CHECK_EQ(shared, 5);
  CHECK_EQ(w.unit("cora").jobs.find(squire)->earned, coraBefore + 5);
}

TEST(a_job_can_require_mastered_entries_by_count_or_by_name) {
  Duel d("teachpts_unlock", false);
  d.c.swap("data/jobs.json", R"("requires": [ { "job": "squire", "level": 2 } ],
    "learn": [ { "skill": "iron_will")", R"("requires": [ { "job": "squire", "mastered": 2 } ],
    "learn": [ { "skill": "iron_will")");
  d.c.swap("data/jobs.json", R"("requires": [ { "job": "squire", "level": 2 } ],
    "jpLevels")", R"("requires": [ { "mastered": "brave" } ],
    "jpLevels")");
  Database db;
  std::string err;
  CHECK(loadGame(d.c.dir, db, err));
  if (!err.empty()) std::printf("  %s\n", err.c_str());
  JobState s;
  s.job = db.findJob("squire");
  s.at(s.job).earned = 200;
  std::string why;
  CHECK(!jobUnlocked(db, s, db.findJob("knight"), &why));
  CHECK_STR(why, "needs 2 Squire entries mastered");
  int guard = -1, shove = -1;
  for (const JobLearnDef& e : db.jobEntries) {
    if (e.refId == "guard") guard = e.flat;
    if (e.refId == "shove") shove = e.flat;
  }
  CHECK(jobLearn(db, s, guard, why));
  CHECK(jobLearn(db, s, shove, why));
  CHECK(jobUnlocked(db, s, db.findJob("knight"), &why));
  // The Mage needs Brave, which only an item teaches: the unit's points decide.
  CHECK(!jobUnlocked(db, s, db.findJob("mage"), &why));
  CHECK_STR(why, "needs Brave mastered");
  std::vector<MasteryPoints> m = {{0, 19}};
  CHECK(!jobUnlocked(db, s, db.findJob("mage"), &why, &m));
  m[0].points = 20;
  CHECK(jobUnlocked(db, s, db.findJob("mage"), &why, &m));
}

TEST(with_spend_jp_off_entries_cannot_be_bought) {
  Duel d("teachpts_nospend", false);
  d.c.swap("game.json", R"("changeIn": "camp+prep" },)", R"("changeIn": "camp+prep", "spendJp": false },)");
  std::string t = d.c.read("data/jobs.json");
  for (const char* cost : {", \"cost\": 30", ", \"cost\": 40", ", \"cost\": 25", ", \"cost\": 60", ", \"cost\": 50"})
    for (size_t at; (at = t.find(cost)) != std::string::npos;) t.erase(at, std::string(cost).size());
  d.c.put("data/jobs.json", t);
  d.c.put("data/items.json", R"({ "items": [ { "id": "manual", "name": "Manual", "kind": "accessory", "teaches": [
    { "skill": "guard", "points": 30 }, { "ability": "shove", "points": 30 }, { "skill": "light_step", "points": 30 },
    { "skill": "iron_will", "points": 30 }, { "ability": "mend", "points": 30 }, { "skill": "focus", "points": 30 } ] } ] })");
  Database db;
  std::string err;
  CHECK(loadGame(d.c.dir, db, err));
  if (!err.empty()) std::printf("  %s\n", err.c_str());
  JobState s;
  s.job = db.findJob("squire");
  s.at(s.job).earned = 500;
  std::string why;
  CHECK(!jobLearn(db, s, 0, why));
  CHECK_STR(why, "learned only from equipment");
}

TEST(points_and_pools_are_saved_with_the_unit_and_refused_when_they_do_not_fit) {
  Duel d("teachpts_save");
  d.give("aldric", "charm");
  CHECK(d.w.apply(Action::useAbility(d.w.id("aldric"), d.ability("fire"), d.w.id("foe"))));  // points, and 4 MP paid
  const std::string text = savetest::stateText(d.b());
  CHECK(text.find("\"mastery\"") != std::string::npos);
  CHECK(text.find("\"pools\"") != std::string::npos);
  Json j;
  saveBattleState(d.b(), j);
  skilltest::World fresh("duel", d.c.dir, false);
  std::string err;
  CHECK(restoreBattleState(fresh.b(), j, err));
  if (!err.empty()) std::printf("  %s\n", err.c_str());
  CHECK_STR(savetest::stateText(fresh.b()), text);
  CHECK(fresh.unit("aldric").mastery == d.unit("aldric").mastery);
  CHECK_EQ(fresh.b().poolOf(fresh.unit("aldric"), 0), 2);
  // Broken values come back as the usual refusals with a path.
  const std::string good = j.dump(0);
  auto tryWith = [&](const char* from, const char* to) -> std::string {
    std::string t = good;
    const size_t at = t.find(from);
    if (at == std::string::npos) return "(missing) " + std::string(from);
    t.replace(at, std::string(from).size(), to);
    Json k;
    std::string perr;
    if (!Json::parse(t, k, perr)) return "(parse) " + perr;
    skilltest::World w("duel", d.c.dir, false);
    std::string e2;
    return restoreBattleState(w.b(), k, e2) ? "" : e2;
  };
  CHECK(saysIn(tryWith("\"points\":5", "\"points\":21"), "\"brave\": points must be from 1 to 20"));
  CHECK(saysIn(tryWith("\"pools\":[2]", "\"pools\":[2,3]"), "pools: has 2 entries, expected 1"));
  CHECK(saysIn(tryWith("\"entry\":1", "\"entry\":0"), "listed twice or out of order"));
}

TEST(the_roster_keeps_points_between_chapters) {
  teachtest::Copy c("teachpts_roster");
  jobtest::Game g;
  CHECK(g.load(c.dir));
  CHECK(g.startChapter());
  g.unit("aldric").mastery = {{0, 7}};
  CHECK(g.winAndFinish());
  RosterUnit* r = g.rosterUnit("aldric");
  CHECK(r != nullptr);
  if (r) CHECK(r->mastery == std::vector<MasteryPoints>({{0, 7}}));
  CHECK(g.startChapter());
  CHECK_EQ(teachPoints(g.unit("aldric").mastery, 0), 7);
}
