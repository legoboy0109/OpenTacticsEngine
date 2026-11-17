// docs/DIFFICULTY.md and docs/SAVES.md: a battle and a playthrough remember their difficulty, only in a game that has
// difficulties; an id that vanished falls back to the default; a change takes effect from the next battle built.
#include "difficulty_fixture.h"
#include "json_edit.h"
#include "core/campaign/difficulty.h"

using namespace ote;

namespace {

// A one-chapter campaign on the arena: its before scene reads the difficulty.
const char* kCampaign = R"({ "name": "Arena", "rules": { "onDefeat": "retry" },
  "chapters": [ { "id": "c1", "name": "One", "map": "skirmish",
    "before": [ { "if": [ { "difficulty": "hard" } ], "then": [ { "setSwitch": "bridge_cut", "to": true } ] },
                { "if": [ { "difficulty": { "atLeast": "normal" } } ], "then": [ { "setSwitch": "met_wanderer", "to": true } ] } ] } ] })";

const Unit& unit(const Battle& b, const char* id) { return b.units[(size_t)b.unitByStoryId(id)]; }

}  // namespace

TEST(difficulty_save_a_battle_remembers_the_difficulty_it_was_built_with) {
  difftest::Game g;
  CHECK(g.load("difficulty_save1"));
  Battle hard;
  std::vector<Event> ev;
  CHECK(g.arena(hard, ev, "hard"));
  Json saved;
  saveBattleState(hard, saved);
  CHECK(saved.has("difficulty") && saved["difficulty"].asString() == "hard");
  // restored onto a battle freshly built at another difficulty: the saved one wins, and what addUnit builds later follows it
  Battle fresh;
  std::vector<Event> ev2;
  MapDef map;
  std::string err;
  CHECK(loadMap(g.g.db, "skirmish", map, err));
  fresh.setDifficulty(0);
  CHECK(fresh.init(g.g.db, map, g.g.formulas, 1, err));
  CHECK_EQ(fresh.difficulty, 0);
  CHECK(restoreBattleState(fresh, saved, err));
  CHECK_EQ(fresh.difficulty, 2);
  CHECK_EQ(fresh.diffXpPercent, 50);
  CHECK(unit(fresh, "foe").level == 5);  // the units are the saved ones, not rebuilt
  CHECK(savetest::stateText(fresh) == savetest::stateText(hard));
}

TEST(difficulty_save_a_vanished_difficulty_falls_back_to_the_default) {
  difftest::Game g;
  CHECK(g.load("difficulty_save2"));
  Battle hard;
  std::vector<Event> ev;
  CHECK(g.arena(hard, ev, "hard"));
  Json saved;
  saveBattleState(hard, saved);
  const Json edited = jsonedit::withValue(saved, {"difficulty"}, jsonedit::str("ghost"));
  Battle fresh;
  MapDef map;
  std::string err;
  CHECK(loadMap(g.g.db, "skirmish", map, err));
  CHECK(fresh.init(g.g.db, map, g.g.formulas, 1, err));
  CHECK(restoreBattleState(fresh, edited, err));
  CHECK_EQ(fresh.difficulty, 1);  // normal, the game's default
  CHECK_EQ(fresh.diffXpPercent, 100);
  // a damaged value is refused, a missing one (a save from before the game had difficulties) keeps what init chose
  Battle b2;
  CHECK(b2.init(g.g.db, map, g.g.formulas, 1, err));
  CHECK(!restoreBattleState(b2, jsonedit::withValue(saved, {"difficulty"}, jsonedit::num(3)), err));
  CHECK(err.find("battle.difficulty") != std::string::npos);
  CHECK(restoreBattleState(b2, jsonedit::without(saved, "difficulty"), err));
  CHECK_EQ(b2.difficulty, 1);
}

TEST(difficulty_save_a_game_without_difficulties_saves_no_difficulty_at_all) {
  savetest::Rig rig;
  CHECK(rig.loadDemo("demo-srpg", "bridge"));
  rig.start();
  Json j;
  saveBattleState(rig.battle, j);
  CHECK(!j.has("difficulty"));
  CHECK_EQ(rig.battle.difficulty, -1);
  difftest::Game g;
  CHECK(g.load("difficulty_save3", "", camptest::mapJson(8, 3, camptest::sides("0", "[]"), camptest::heroesOnBlue(""))));
  Playthrough pt;
  CHECK(pt.begin(g.g.db));
  CHECK(pt.difficulty.empty());
  Json p;
  savePlaythrough(g.g.db, pt, p);
  CHECK(!p.has("difficulty"));
}

TEST(difficulty_save_a_playthrough_stores_and_restores_its_difficulty) {
  difftest::Game g;
  CHECK(g.load("difficulty_save4", difftest::kDifficulties, "", kCampaign));
  Playthrough pt;
  CHECK(pt.begin(g.g.db));
  CHECK_STR(pt.difficulty, "normal");  // the default until the player chooses
  std::string why;
  CHECK(setPlaythroughDifficulty(g.g.db, pt, "hard", true, why));  // New Game: any id
  Json p;
  savePlaythrough(g.g.db, pt, p);
  CHECK(p["difficulty"].asString() == "hard");
  Playthrough back;
  std::string err;
  CHECK(restorePlaythrough(g.g.db, p, back, err));
  CHECK_STR(back.difficulty, "hard");
  // an id the game no longer has: the default
  Playthrough ghost;
  CHECK(restorePlaythrough(g.g.db, jsonedit::withValue(p, {"difficulty"}, jsonedit::str("ghost")), ghost, err));
  CHECK_STR(ghost.difficulty, "normal");
  CHECK(!restorePlaythrough(g.g.db, jsonedit::withValue(p, {"difficulty"}, jsonedit::num(2)), ghost, err));
  // a save without the key in a game that has difficulties (it had none when saved): the default
  Playthrough old;
  CHECK(restorePlaythrough(g.g.db, jsonedit::without(p, "difficulty"), old, err));
  CHECK_STR(old.difficulty, "normal");
}

TEST(difficulty_save_changing_it_later_follows_the_games_rule) {
  difftest::Game g;
  CHECK(g.load("difficulty_save5", difftest::kDifficulties, "", kCampaign));
  Playthrough pt;
  CHECK(pt.begin(g.g.db));
  std::string why;
  CHECK(setPlaythroughDifficulty(g.g.db, pt, "hard", true, why));
  CHECK(setPlaythroughDifficulty(g.g.db, pt, "normal", false, why) && pt.difficulty == "normal");   // lowerOnly: down is fine
  CHECK(!setPlaythroughDifficulty(g.g.db, pt, "hard", false, why) && pt.difficulty == "normal");    // up is not
  CHECK(why.find("easier") != std::string::npos);
  CHECK(!setPlaythroughDifficulty(g.g.db, pt, "ghost", false, why));
  difftest::Game never;
  std::string neverDiffs = difftest::kDifficulties;
  neverDiffs.replace(neverDiffs.find("\"lowerOnly\""), 11, "\"never\"");
  CHECK(never.load("difficulty_save6", neverDiffs, "", kCampaign));
  Playthrough p2;
  CHECK(p2.begin(never.g.db));
  CHECK(!setPlaythroughDifficulty(never.g.db, p2, "easy", false, why) && p2.difficulty == "normal");
  CHECK(why.find("does not allow") != std::string::npos);
  CHECK(setPlaythroughDifficulty(never.g.db, p2, "normal", false, why));  // "changing" to what it is is nothing
  difftest::Game freeG;
  std::string d = difftest::kDifficulties;
  d.replace(d.find("\"lowerOnly\""), 11, "\"free\"");
  CHECK(freeG.load("difficulty_save7", d, "", kCampaign));
  Playthrough p3;
  CHECK(p3.begin(freeG.g.db));
  CHECK(setPlaythroughDifficulty(freeG.g.db, p3, "hard", false, why) && p3.difficulty == "hard");
  CHECK(setPlaythroughDifficulty(freeG.g.db, p3, "easy", false, why) && p3.difficulty == "easy");
}

TEST(difficulty_save_a_chapter_battle_is_built_with_the_playthroughs_difficulty_and_a_change_waits) {
  difftest::Game g;
  CHECK(g.load("difficulty_save8", difftest::kDifficulties, "", kCampaign));
  Playthrough pt;
  CHECK(pt.begin(g.g.db));
  std::string why;
  CHECK(setPlaythroughDifficulty(g.g.db, pt, "hard", true, why));
  std::vector<SceneLine> lines;
  CHECK(runScene(g.g.db, pt, SceneKind::Before, lines));  // the before scene reads the difficulty
  CHECK(pt.switches[(size_t)g.g.db.findSwitch("bridge_cut")] && pt.switches[(size_t)g.g.db.findSwitch("met_wanderer")]);
  Battle b;
  std::vector<Event> ev;
  std::string err;
  CHECK(startChapterBattle(g.g.db, pt, g.g.formulas, 1, b, ev, err));
  CHECK_EQ(b.difficulty, 2);
  CHECK_EQ(unit(b, "foe").level, 5);
  // changing it now (the game allows lower only) does not touch the battle in progress
  CHECK(setPlaythroughDifficulty(g.g.db, pt, "easy", false, why));
  CHECK_EQ(b.difficulty, 2);
  CHECK_EQ(unit(b, "foe").level, 5);
  // the campaign save carries both: the playthrough's (now easy) and the battle's own (hard)
  Json save;
  CHECK(buildCampaignSave(g.g.db, "g", pt, &b, 1, save, err));
  CHECK(save["playthrough"]["difficulty"].asString() == "easy" && save["battle"]["difficulty"].asString() == "hard");
  Playthrough pt2;
  Battle b2;
  bool hasBattle = false;
  CHECK(restoreCampaignSave(g.g.db, g.g.formulas, "g", save, pt2, b2, hasBattle, err));
  CHECK(hasBattle && pt2.difficulty == "easy" && b2.difficulty == 2 && unit(b2, "foe").level == 5);
  // the next battle built (a retry) uses the new one
  Battle next;
  CHECK(startChapterBattle(g.g.db, pt, g.g.formulas, 1, next, ev, err));
  CHECK(next.difficulty == 0 && unit(next, "foe").level == 1);
}

TEST(difficulty_save_free_battles_use_the_chosen_difficulty_too) {
  difftest::Game g;
  CHECK(g.load("difficulty_save9"));
  Battle b;
  std::vector<Event> ev;
  CHECK(g.arena(b, ev, "easy"));
  CHECK_EQ(b.difficulty, 0);
  Json j;
  saveBattleState(b, j);
  CHECK(j["difficulty"].asString() == "easy");
}
