// The campaign rules (docs/EVENTS.md "Campaign"): scenes, starting a chapter's battle from the roster,
// taking the result back, the next chapter, fallen units, defeat, recruits and free battles.
#include "campaign_run.h"

using namespace camptest;

namespace {

int atkIndex(const Database& db) { return db.findStat("atk"); }

// Every unit's id equals its position and the occupancy agrees (what removing a unit must keep true).
bool consistent(const Battle& b) {
  for (size_t i = 0; i < b.units.size(); ++i) {
    if (b.units[i].id != (int)i) return false;
    if (b.units[i].alive && b.occupant[(size_t)b.units[i].cell] != (int)i) return false;
  }
  int living = 0;
  for (int o : b.occupant) living += o >= 0;
  int alive = 0;
  for (const Unit& u : b.units) alive += u.alive;
  return living == alive;
}

}  // namespace

TEST(campaign_begin_state) {
  Run r;
  CHECK(r.begin("camp_play"));
  CHECK(r.pt.phase == Phase::Before && r.pt.chapter == 0 && r.pt.active());
  CHECK(r.pt.roster.empty() && r.pt.sides.empty());
  CHECK_EQ((int)r.pt.switches.size(), 2);
  CHECK_EQ((int)r.pt.variables.size(), 2);
  CHECK_STR(currentChapter(r.g.db, r.pt)->id, "ch1");
}

TEST(campaign_scenes_give_lines_and_apply_at_once) {
  Run r;
  CHECK(r.begin("camp_play"));
  CHECK(!r.scene(SceneKind::After));  // not due yet
  CHECK(r.pt.phase == Phase::Before);
  CHECK(r.scene(SceneKind::Before));
  CHECK_EQ((int)r.lines.size(), 2);
  // eirik is not in the party yet: no name, like a Dialogue entry with no unit; the id is kept.
  CHECK_STR(r.lines[0].speakerId, "eirik");
  CHECK_STR(r.lines[0].speaker, "");
  CHECK_STR(r.lines[0].text, "We hold the ford.");
  CHECK_STR(r.lines[1].speaker, "Narrator");
  CHECK_STR(r.lines[1].portrait, "narr");
  CHECK_EQ(r.pt.variables[(size_t)r.g.db.findVariable("chapters")], 1);
  CHECK(r.pt.phase == Phase::Battle);
  CHECK(!r.scene(SceneKind::Before));  // a scene runs once; retry never replays it
  CHECK_EQ(r.pt.variables[(size_t)r.g.db.findVariable("chapters")], 1);
}

TEST(campaign_battle_needs_the_battle_phase) {
  Run r;
  CHECK(r.begin("camp_play"));
  std::string err;
  Battle b;
  CHECK(!prepareChapterBattle(r.g.db, r.pt, r.g.formulas, 1, b, err));  // the before scene is still due
  CHECK(!err.empty());
}

TEST(campaign_ford_start_recruits_and_adds_named_units) {
  Run r;
  CHECK(r.begin("camp_play") && r.startBattle());
  CHECK_EQ((int)r.b.units.size(), 7);  // 3 blue, wanderer, scout, 2 grunts
  CHECK_EQ(r.b.units[(size_t)r.unit("wanderer")].side, 0);  // changeSide
  CHECK_STR(r.b.units[(size_t)r.unit("scout")].name, "Scout");  // addUnit
  CHECK(r.b.storyState.switches[(size_t)r.g.db.findSwitch("met_wanderer")]);
  CHECK(consistent(r.b));
}

TEST(campaign_won_chapter_carries_everything_to_the_next) {
  Run r;
  CHECK(r.begin("camp_play") && r.startBattle());
  playOut(r.b, r.ev);  // the AI plays both sides
  CHECK(r.b.over && r.b.winnerTeam == r.b.humanTeam());
  const Database& db = r.g.db;
  int eirik = r.unit("eirik");
  CHECK(r.b.units[(size_t)eirik].alive);  // the grunts cannot hurt a hero
  // Known values, so the carry-over can be compared exactly.
  Unit& e = r.b.units[(size_t)eirik];
  e.level = 3;
  e.xp = 40;
  e.stats[(size_t)atkIndex(db)] += 2;
  e.items.clear();
  ItemStack ring, potion;
  ring.item = db.findItem("ring");
  potion.item = db.findItem("potion");
  potion.uses = 1;
  e.items = {ring, potion};
  ItemStack tonic;
  tonic.item = db.findItem("tonic");
  r.b.sides[0].storage.push_back(tonic);
  r.b.sides[0].resources[0] = 130;
  r.b.storyState.variables[(size_t)db.findVariable("kills")] = 5;
  const int atk = e.stats[(size_t)atkIndex(db)];
  bool wandererAlive = r.b.units[(size_t)r.unit("wanderer")].alive;

  CHECK(r.winAndFinish() == ChapterResult::Won);
  CHECK(r.pt.phase == Phase::After);
  const RosterUnit* re = r.pt.findUnit("eirik");
  CHECK(re != nullptr);
  if (re) {
    CHECK(re->level == 3 && re->xp == 40 && !re->gone);
    CHECK_EQ(re->stats[(size_t)atkIndex(db)], atk);
    CHECK_EQ((int)re->items.size(), 2);
    CHECK(re->items[1].item == potion.item && re->items[1].uses == 1);
    CHECK_STR(re->side, "blue");
  }
  // Recruits and the unit the event added joined the party; the unnamed ones did not.
  CHECK(r.pt.findUnit("lyra") && r.pt.findUnit("wanderer") && r.pt.findUnit("scout"));
  CHECK_EQ((int)r.pt.roster.size(), 4);
  CHECK_STR(r.pt.findUnit("wanderer")->side, "blue");
  CHECK_EQ(r.pt.findUnit("wanderer")->gone ? 1 : 0, wandererAlive ? 0 : 1);
  CHECK(r.pt.switches[(size_t)db.findSwitch("met_wanderer")] == 1);
  CHECK_EQ(r.pt.variables[(size_t)db.findVariable("kills")], 5);

  // The after scene: names come from the roster, gifts go to the side's carried resources.
  CHECK(r.scene(SceneKind::After));
  CHECK_STR(r.lines[0].speaker, "Lyra");
  CHECK_STR(r.lines[1].speaker, wandererAlive ? "Eirik" : "");
  CHECK_STR(r.lines[1].text, wandererAlive ? "Welcome, wanderer." : "Alone again.");
  CHECK(r.pt.phase == Phase::Before && r.pt.chapter == 1);  // no goto, no next: the following chapter
  CHECK_EQ(r.pt.sides[0].resources[0], 155);                // 130 + the scene's 25

  // Chapter 2: the roster's version replaces the map's, at full strength, on the map's square.
  CHECK(r.startBattle(2));
  CHECK(consistent(r.b));
  const Unit& e2 = r.b.units[(size_t)r.unit("eirik")];
  CHECK(e2.level == 3 && e2.xp == 40 && e2.name == "Eirik");
  CHECK_EQ(e2.stats[(size_t)atkIndex(db)], atk);
  CHECK(e2.items.size() == 2 && e2.items[0].item == ring.item && e2.items[1].uses == 1);
  CHECK_EQ(r.b.maxHp(e2), 24);  // 20 + the ring's 4
  CHECK_EQ(e2.hp, 24);
  CHECK_EQ(e2.cell, r.b.grid.at(0, 1));
  CHECK_EQ(r.b.sides[0].resources[0], 205);  // carried 155 + the map's 50
  CHECK_EQ((int)r.b.sides[0].storage.size(), 3);  // carried [potion, tonic] then the map's [tonic]
  if (r.b.sides[0].storage.size() == 3)
    CHECK(r.b.sides[0].storage[0].item == potion.item && r.b.sides[0].storage[1].item == tonic.item &&
          r.b.sides[0].storage[2].item == tonic.item);
  CHECK(r.b.storyState.switches[(size_t)db.findSwitch("met_wanderer")]);
  CHECK_EQ(r.b.storyState.variables[(size_t)db.findVariable("kills")], 5);
  CHECK_EQ(r.b.storyState.variables[(size_t)db.findVariable("chapters")], 2);  // both before scenes ran
  const Unit& scout = r.b.units[(size_t)r.unit("scout")];
  CHECK_EQ(scout.level, r.pt.findUnit("scout")->level);  // not the map's level 1
  CHECK_EQ(r.b.sides[1].resources[0], 0);  // the computer's side carries nothing
  CHECK(r.b.units[(size_t)r.unit("brute")].level == 1 && r.b.units[(size_t)r.unit("brute")].name == "Brute");
}

TEST(campaign_goto_branches_on_a_switch_set_in_battle) {
  // With the bridge cut (eirik walks onto the square that sets it) the story goes to ch3b.
  {
    Run r;
    CHECK(r.begin("camp_play") && r.startBattle());
    r.winAndFinish();
    r.scene(SceneKind::After);
    CHECK(r.startBattle());
    int sw = r.g.db.findSwitch("bridge_cut");
    CHECK(!r.b.storyState.switches[(size_t)sw]);
    CHECK(r.b.apply(Action::move(r.unit("eirik"), r.b.grid.at(3, 1)), r.ev));
    CHECK(r.b.storyState.switches[(size_t)sw]);
    CHECK(r.winAndFinish() == ChapterResult::Won);
    CHECK(r.pt.switches[(size_t)sw]);
    CHECK(r.scene(SceneKind::After));
    CHECK(r.pt.chapter == 3 && r.pt.phase == Phase::Before);  // ch3b
    CHECK(r.scene(SceneKind::Before));
    CHECK_STR(r.lines[0].text, "The keep, by night.");
    // ch3b has no next: the chapter that follows it in the list.
    CHECK(r.startBattle());
    CHECK(r.winAndFinish() == ChapterResult::Won && r.scene(SceneKind::After));
    CHECK(r.pt.chapter == 4);
  }
  // Without it, the following chapter (ch3a), whose "next" skips ch3b.
  {
    Run r;
    CHECK(r.begin("camp_play") && r.startBattle());
    r.winAndFinish();
    r.scene(SceneKind::After);
    CHECK(r.startBattle() && r.winAndFinish() == ChapterResult::Won && r.scene(SceneKind::After));
    CHECK(r.pt.chapter == 2);
    CHECK(r.scene(SceneKind::Before));
    CHECK_STR(r.lines[0].text, "The keep, by the road.");
    CHECK(r.startBattle() && r.winAndFinish() == ChapterResult::Won && r.scene(SceneKind::After));
    CHECK(r.pt.chapter == 4 && r.pt.phase == Phase::Before);
  }
}

TEST(campaign_the_last_goto_wins_and_overrides_next) {
  const char* json = R"({ "chapters": [
    { "id":"a","map":"ford","next":"c","after":[ {"goto":"d"}, {"if":[{"switch":"bridge_cut","is":false}],"then":[{"goto":"b"}]} ] },
    { "id":"b","map":"mill" }, { "id":"c","map":"keep" }, { "id":"d","map":"skirmish" } ] })";
  Run r;
  CHECK(r.begin("camp_play", json) && r.startBattle());
  r.winAndFinish();
  CHECK(r.scene(SceneKind::After));
  CHECK_EQ(r.pt.chapter, 1);  // the second goto (b) ran last and beat the first (d) and "next" (c)
}

TEST(campaign_the_last_chapter_finishes_the_playthrough) {
  const char* json = R"({ "chapters": [ {"id":"a","map":"skirmish","after":[{"say":"Fin."}]} ] })";
  Run r;
  CHECK(r.begin("camp_play", json) && r.startBattle());
  CHECK(r.winAndFinish() == ChapterResult::Won);
  CHECK(r.scene(SceneKind::After));
  CHECK(r.pt.phase == Phase::Finished && !r.pt.active() && r.pt.chapter == 0);
  CHECK(currentChapter(r.g.db, r.pt) == nullptr);
  CHECK(!r.scene(SceneKind::Before) && !r.scene(SceneKind::After));
  std::string err;
  Battle b;
  CHECK(!prepareChapterBattle(r.g.db, r.pt, r.g.formulas, 1, b, err));
}

TEST(campaign_fallen_gone_removes_the_unit_for_good) {
  Run r;
  CHECK(r.begin("camp_play") && r.startBattle());
  r.kill("lyra");
  CHECK(r.winAndFinish() == ChapterResult::Won);
  const RosterUnit* lyra = r.pt.findUnit("lyra");
  CHECK(lyra != nullptr && lyra->gone);
  r.scene(SceneKind::After);
  CHECK(r.startBattle());  // the mill: lyra is on its map but gone
  CHECK(consistent(r.b));
  CHECK_EQ(r.unit("lyra"), -1);
  CHECK(r.b.unitAt(r.b.grid.at(0, 0)) == -1);  // her square is free
  CHECK_EQ((int)r.b.units.size(), 4);          // eirik, wanderer, scout, brute
  // Scenes know: dead = in the roster and gone.
  const char* json = R"({ "chapters": [ {"id":"a","map":"ford","after":[
      {"if":[{"dead":"lyra"}],"then":[{"say":"Lyra fell."}]}, {"if":[{"alive":"lyra"}],"then":[{"say":"Lyra lives."}]},
      {"if":[{"dead":"nobody"}],"then":[{"say":"Nobody?"}]} ] } ] })";
  Run s;
  CHECK(s.begin("camp_play", json) && s.startBattle());
  s.kill("lyra");
  s.winAndFinish();
  s.scene(SceneKind::After);
  CHECK_STR(s.said(), "|Lyra fell.;");
}

TEST(campaign_fallen_return_keeps_the_roster_version) {
  const char* json = R"({ "rules": {"fallen":"return"}, "chapters": [ {"id":"a","map":"ford"}, {"id":"b","map":"mill"} ] })";
  Run r;
  CHECK(r.begin("camp_play", json) && r.startBattle());
  r.b.units[(size_t)r.unit("eirik")].level = 5;
  r.winAndFinish();
  CHECK_EQ(r.pt.findUnit("eirik")->level, 5);
  r.scene(SceneKind::After);
  CHECK(r.startBattle());
  r.b.units[(size_t)r.unit("eirik")].level = 9;  // gains this chapter ...
  r.b.units[(size_t)r.unit("eirik")].xp = 77;
  r.kill("eirik");  // ... but falls
  r.b.units[(size_t)r.unit("lyra")].level = 4;  // and a survivor does gain
  CHECK(r.winAndFinish() == ChapterResult::Won);
  const RosterUnit* e = r.pt.findUnit("eirik");
  CHECK(e != nullptr && !e->gone && e->level == 5 && e->xp != 77);  // as at the start of the chapter, nothing gained
  CHECK_EQ(r.pt.findUnit("lyra")->level, 4);
  // A unit that fell in the very chapter it first appeared in was never in the roster: it comes back as its map says.
  Run s;
  CHECK(s.begin("camp_play", json) && s.startBattle());
  s.kill("lyra");
  s.winAndFinish();
  CHECK(s.pt.findUnit("lyra") == nullptr);
  s.scene(SceneKind::After);
  CHECK(s.startBattle());
  CHECK(s.unit("lyra") >= 0);
}

TEST(campaign_a_unit_that_left_for_another_side_leaves_the_roster) {
  Run r;
  CHECK(r.begin("camp_play") && r.startBattle());
  r.winAndFinish();
  CHECK(r.pt.findUnit("scout"));
  r.scene(SceneKind::After);
  CHECK(r.startBattle());
  r.b.units[(size_t)r.unit("scout")].side = 1;  // defected to the computer's side
  r.winAndFinish();
  CHECK(r.pt.findUnit("scout") == nullptr);
  CHECK(r.pt.findUnit("lyra") != nullptr);
}

