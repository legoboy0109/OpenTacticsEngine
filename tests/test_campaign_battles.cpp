// The campaign rules, part 2: defeat, a real fallen unit, roster dressing details, free battles and
// a whole playthrough run twice (docs/EVENTS.md "Campaign").
#include "campaign_run.h"

using namespace camptest;

namespace {

bool consistent(const Battle& b) {
  for (size_t i = 0; i < b.units.size(); ++i)
    if (b.units[i].id != (int)i || (b.units[i].alive && b.occupant[(size_t)b.units[i].cell] != (int)i)) return false;
  return true;
}

}  // namespace

TEST(campaign_a_real_defeat_in_battle_makes_the_unit_gone) {
  // fodder (1 hit point) stands next to a grunt: the grunt strikes it down, the hero finishes the grunt.
  Run r;
  r.g.keepJson = mapJson(4, 2, sides("0", "[]"),
                         R"([ {"id":"fodder","type":"ranger","side":"blue","x":0,"y":0,"stats":{"hp":1}},
                              {"id":"eirik","type":"hero","side":"blue","x":1,"y":1},
                              {"type":"grunt","side":"red","x":1,"y":0} ])");
  CHECK(r.begin("camp_play", R"({ "chapters": [ {"id":"a","map":"keep"}, {"id":"b","map":"keep"} ] })"));
  CHECK(r.startBattle());
  int grunt = -1;
  for (const Unit& u : r.b.units)
    if (u.side == 1) grunt = u.id;
  CHECK(r.b.apply(Action::endTurn(), r.ev));
  CHECK(r.b.apply(Action::attack(grunt, r.unit("fodder"), 0), r.ev));
  CHECK(!r.b.units[(size_t)r.unit("fodder")].alive);
  CHECK(r.b.apply(Action::endTurn(), r.ev));
  CHECK(r.b.apply(Action::attack(r.unit("eirik"), grunt, 0), r.ev));
  CHECK(r.b.over && r.b.winnerTeam == r.b.humanTeam());
  CHECK(finishChapterBattle(r.g.db, r.pt, r.b) == ChapterResult::Won);
  CHECK(r.pt.findUnit("fodder") && r.pt.findUnit("fodder")->gone);
  CHECK(r.pt.findUnit("eirik") && !r.pt.findUnit("eirik")->gone);
  r.scene(SceneKind::After);
  CHECK(r.startBattle());
  CHECK_EQ(r.unit("fodder"), -1);
  CHECK(r.unit("eirik") >= 0 && consistent(r.b));
}

TEST(campaign_defeat_retry_leaves_the_playthrough_as_it_was) {
  Run r;
  CHECK(r.begin("camp_play") && r.startBattle());
  CHECK(finishChapterBattle(r.g.db, r.pt, r.b) == ChapterResult::NotOver);
  std::string before = r.state();
  r.b.units[(size_t)r.unit("eirik")].level = 8;  // whatever happened in the lost battle ...
  r.b.sides[0].resources[0] = 999;
  r.b.storyState.variables[0] = 77;
  r.decide(false);
  CHECK(finishChapterBattle(r.g.db, r.pt, r.b) == ChapterResult::Retry);
  CHECK_STR(r.state(), before);  // ... is lost with it
  CHECK(r.pt.phase == Phase::Battle && r.pt.active());
  CHECK(!r.scene(SceneKind::Before));  // the before scene is not replayed
  CHECK(r.startBattle());              // the chapter can be started again
  CHECK_EQ(r.b.sides[0].resources[0], 100);
  // A draw counts as a defeat.
  r.b.over = true;
  r.b.winnerTeam = -1;
  CHECK(finishChapterBattle(r.g.db, r.pt, r.b) == ChapterResult::Retry);
}

TEST(campaign_defeat_end_ends_the_campaign) {
  const char* json = R"({ "rules": {"onDefeat":"end"}, "chapters": [ {"id":"a","map":"ford"}, {"id":"b","map":"mill"} ] })";
  Run r;
  CHECK(r.begin("camp_play", json) && r.startBattle());
  r.decide(false);
  CHECK(finishChapterBattle(r.g.db, r.pt, r.b) == ChapterResult::Ended);
  CHECK(r.pt.phase == Phase::Over && !r.pt.active());
  CHECK(r.pt.roster.empty());
  CHECK(!r.scene(SceneKind::After) && !r.scene(SceneKind::Before));
  CHECK(finishChapterBattle(r.g.db, r.pt, r.b) == ChapterResult::WrongPhase);
  std::string err;
  Battle b;
  CHECK(!prepareChapterBattle(r.g.db, r.pt, r.g.formulas, 1, b, err));
}

TEST(campaign_wrong_phase_finishing_changes_nothing) {
  Run r;
  CHECK(r.begin("camp_play"));
  Battle b;
  b.over = true;
  CHECK(finishChapterBattle(r.g.db, r.pt, b) == ChapterResult::WrongPhase);  // still before the scene
  CHECK(r.pt.phase == Phase::Before);
}

TEST(campaign_roster_dresses_units_that_addUnit_events_create_later) {
  Run r;
  CHECK(r.begin("camp_play"));
  RosterUnit scout;  // a veteran from "an earlier chapter"
  scout.id = "scout";
  scout.type = r.g.db.findUnitType("hero");
  scout.name = "Old Scout";
  scout.level = 7;
  scout.xp = 12;
  scout.stats = {30, 9, 3, 4};
  scout.side = "blue";
  r.pt.roster.push_back(scout);
  CHECK(r.startBattle());
  const Unit& u = r.b.units[(size_t)r.unit("scout")];  // created by the ford's start event
  CHECK(u.level == 7 && u.name == "Old Scout" && u.type == scout.type && u.hp == 30);
  CHECK(consistent(r.b));
  // A scout that is gone is not added at all.
  Run s;
  CHECK(s.begin("camp_play"));
  scout.gone = true;
  s.pt.roster.push_back(scout);
  CHECK(s.startBattle());
  CHECK_EQ(s.unit("scout"), -1);
  CHECK_EQ((int)s.b.units.size(), 6);
}

TEST(campaign_only_units_on_human_sides_are_dressed_from_the_roster) {
  Run r;
  CHECK(r.begin("camp_play"));
  RosterUnit w;  // the ford's "wanderer" starts on the computer's side
  w.id = "wanderer";
  w.type = r.g.db.findUnitType("brute");
  w.name = "Veteran";
  w.level = 5;
  w.stats = {30, 12, 6, 3};
  r.pt.roster.push_back(w);
  CHECK(r.startBattle());
  // The map's side decides at placement, so he stays as the map says even though an event recruits him
  // afterwards (a roster entry belongs to a unit that was ours when the chapter began).
  const Unit& u = r.b.units[(size_t)r.unit("wanderer")];
  CHECK(u.name == "Wanderer" && u.level == 1);
}

TEST(campaign_free_battle_never_touches_a_playthrough) {
  Run r;
  CHECK(r.begin("camp_play") && r.startBattle());
  r.b.units[(size_t)r.unit("eirik")].level = 6;
  r.winAndFinish();
  std::string before = r.state();
  MapDef map;
  std::string err;
  Battle free;
  std::vector<Event> ev;
  CHECK(loadMap(r.g.db, "mill", map, err) && free.init(r.g.db, map, r.g.formulas, 3, err));
  free.start(ev);
  CHECK_EQ(free.units[(size_t)free.unitByStoryId("eirik")].level, 1);  // the map's own units, no roster
  CHECK_EQ(free.sides[0].resources[0], 50);
  CHECK(free.roster == nullptr);
  CHECK_STR(r.state(), before);
}

TEST(campaign_three_chapters_play_through_the_same_every_time) {
  // AI vs AI all the way. The brute of the mill wins the first time (a real defeat, so the chapter is
  // retried); the second try is called won so the story can go on.
  auto play = [](std::string& out, int& retries) {
    Run r;
    if (!r.begin("camp_play")) return false;
    int guard = 0, tries = 0, last = -1;
    while (r.pt.active() && ++guard < 20) {
      if (r.pt.phase == Phase::After) {
        r.scene(SceneKind::After);
        out += r.said() + "\n" + r.state() + "\n";
        continue;
      }
      tries = r.pt.chapter == last ? tries + 1 : 0;
      last = r.pt.chapter;
      if (!r.startBattle((uint32_t)guard)) return false;
      if (tries == 0) playOut(r.b, r.ev);
      if (tries > 0 || !r.b.over) r.decide(true);
      out += savetest::stateText(r.b) + "\n";
      if (finishChapterBattle(r.g.db, r.pt, r.b) == ChapterResult::Retry) ++retries;
      out += r.state() + "\n";
    }
    return r.pt.phase == Phase::Finished;
  };
  std::string a, b;
  int ra = 0, rb = 0;
  CHECK(play(a, ra));
  CHECK(play(b, rb));
  CHECK(!a.empty());
  CHECK_STR(a, b);
  CHECK_EQ(ra, rb);
}
