// Story events and research (docs/EVENTS.md, docs/ECONOMY.md): the "researched" condition and the
// "grantResearch" command, in a battle and in the loader.
#include "research_fixture.h"

using namespace ote;
using namespace restest;

namespace {

std::string startEvent(const std::string& commands, const std::string& cond = "") {
  return "[{\"id\":\"e\",\"when\":{\"battle\":\"start\"}," + (cond.empty() ? "" : "\"if\":" + cond + ",") + "\"do\":" + commands + "}]";
}

std::string loadProblem(const std::string& events) {
  ResearchWorld w;
  return w.load(events);
}

}  // namespace

TEST(story_grant_research_finishes_a_technology_at_once_for_nothing) {
  ResearchWorld w;
  CHECK(w.play(startEvent(R"([{"grantResearch":"masonry","side":"blue"}])")));
  CHECK(w.battle.hasTechnology(0, Masonry));
  CHECK(!w.battle.hasTechnology(1, Masonry));
  CHECK_EQ(w.count(EventType::ResearchFinished), 1);
  const Event* e = w.last(EventType::ResearchFinished);
  CHECK(e && e->a == 0 && e->b == (int)Masonry);
  CHECK_EQ(w.count(EventType::ResourceChanged), 1);  // only blue's turn income: nothing was paid
  CHECK_EQ(w.funds(), 2000 + 100 + 100);              // city + castle income
  CHECK_EQ(w.count(EventType::ResearchStarted), 0);
}

TEST(story_grant_research_twice_says_nothing_the_second_time) {
  ResearchWorld w;
  CHECK(w.play(startEvent(R"([{"grantResearch":"tools","side":"blue"},{"grantResearch":"tools","side":"blue"}])")));
  CHECK_EQ(w.count(EventType::ResearchFinished), 1);
}

TEST(story_grant_research_finishes_the_one_under_way) {
  ResearchWorld w;
  std::string events = R"([{"id":"g","when":{"turn":2},"do":[{"grantResearch":"tools","side":"blue"}]}])";
  CHECK(w.play(events));
  CHECK(w.act(Action::research(Tools)));
  CHECK_EQ(w.battle.researching(0), (int)Tools);
  w.events.clear();
  CHECK(w.act(Action::endTurn()));
  CHECK(w.act(Action::endTurn()));  // round 2 for blue: the event runs, and the countdown would have finished it anyway
  CHECK(w.battle.hasTechnology(0, Tools));
  CHECK_EQ(w.battle.researching(0), -1);
  CHECK_EQ(w.count(EventType::ResearchFinished), 1);  // once, not twice
}

TEST(story_grant_research_in_the_middle_clears_progress_and_applies_bonuses) {
  ResearchWorld w;
  CHECK(w.play(R"([{"id":"g","when":{"turn":1,"side":"red"},"do":[{"grantResearch":"frailty","side":"blue"}]}])"));
  CHECK(w.act(Action::research(Frailty)));  // under way (1 turn)
  CHECK_EQ(w.battle.units[0].hp, 10);
  CHECK(w.act(Action::endTurn()));  // red's turn: the event grants it
  CHECK(w.battle.hasTechnology(0, Frailty));
  CHECK_EQ(w.battle.researching(0), -1);
  CHECK_EQ(w.battle.units[0].hp, 7);  // maximum 10 - 3: hit points clamp
}

TEST(story_grant_research_unlocks_unit_types) {
  ResearchWorld w;
  CHECK(w.play(startEvent(R"([{"grantResearch":"armor","side":"blue"}])")));
  CHECK(w.battle.canBuild(w.at(4, 0), 2));
}

TEST(story_researched_condition_gates_an_event_per_side) {
  ResearchWorld w;
  std::string events = R"([
    {"id":"a","when":{"turn":1},"if":[{"researched":"tools","side":"blue"}],"do":[{"setSwitch":"saved","to":true}]},
    {"id":"b","when":{"turn":2},"if":[{"researched":"tools","side":"blue"}],"do":[{"setSwitch":"alarm","to":true}]},
    {"id":"c","when":{"turn":2},"if":[{"researched":"tools","side":"red"}],"do":[{"addVariable":"kills","by":1}]}])";
  CHECK(w.play(events));
  CHECK(!w.sw("saved"));  // not yet
  CHECK(w.act(Action::research(Armor)));  // an instant one: not "tools"
  CHECK(w.act(Action::research(Tools)));
  CHECK(w.nextTurn());   // tools finishes at the start of blue's second turn; the turn event comes after it
  CHECK(w.sw("alarm"));
  CHECK_EQ(w.var("kills"), 0);  // red has not researched it
}

TEST(story_researched_condition_works_inside_if_commands) {
  ResearchWorld w;
  CHECK(w.play(startEvent(R"([{"grantResearch":"tools","side":"red"},
      {"if":[{"researched":"tools","side":"red"}],"then":[{"setVariable":"gold","to":5}],"else":[{"setVariable":"gold","to":9}]},
      {"if":[{"researched":"tools","side":"blue"}],"then":[{"setVariable":"kills","to":5}],"else":[{"setVariable":"kills","to":9}]}])")));
  CHECK_EQ(w.var("gold"), 5);
  CHECK_EQ(w.var("kills"), 9);
}

TEST(story_changing_side_clamps_hit_points_to_the_new_sides_maximum) {
  ResearchWorld w;
  CHECK(w.play(R"([
    {"id":"f","when":{"battle":"start"},"do":[{"grantResearch":"frailty","side":"blue"}]},
    {"id":"c","when":{"turn":2},"do":[{"changeSide":"foe","side":"blue"}]}])"));
  CHECK_EQ(w.battle.units[1].hp, 10);
  CHECK(w.nextTurn());
  CHECK_EQ(w.battle.units[1].side, 0);
  CHECK_EQ(w.battle.units[1].hp, 7);  // blue's research lowers the maximum of its new unit
}

// ---- the loader ----
TEST(story_loader_reads_researched_and_grant_research) {
  CHECK_STR(loadProblem(startEvent(R"([{"grantResearch":"tools","side":"blue"}])", R"([{"researched":"tools","side":"red"}])")), "");
}
TEST(story_loader_refuses_researched_with_an_unknown_technology) {
  std::string err = loadProblem(startEvent("[]", R"([{"researched":"magic","side":"blue"}])"));
  CHECK(has(err, "unknown technology \"magic\"") && has(err, "event \"e\""));
}
TEST(story_loader_refuses_researched_with_an_unknown_side) {
  CHECK(has(loadProblem(startEvent("[]", R"([{"researched":"tools","side":"purple"}])")), "unknown side \"purple\""));
}
TEST(story_loader_refuses_researched_without_a_side) {
  CHECK(has(loadProblem(startEvent("[]", R"([{"researched":"tools"}])")), "\"side\""));
}
TEST(story_loader_refuses_researched_with_other_keys) {
  CHECK(has(loadProblem(startEvent("[]", R"([{"researched":"tools","side":"blue","is":false}])")), "unknown key \"is\""));
}
TEST(story_loader_refuses_researched_that_is_not_text) {
  CHECK(has(loadProblem(startEvent("[]", R"([{"researched":5,"side":"blue"}])")), "unknown technology"));
}
TEST(story_loader_refuses_grant_research_with_an_unknown_technology) {
  std::string err = loadProblem(startEvent(R"([{"grantResearch":"magic","side":"blue"}])"));
  CHECK(has(err, "unknown technology \"magic\"") && has(err, "command 1 (grantResearch)"));
}
TEST(story_loader_refuses_grant_research_with_an_unknown_side) {
  CHECK(has(loadProblem(startEvent(R"([{"grantResearch":"tools","side":"purple"}])")), "unknown side \"purple\""));
}
TEST(story_loader_refuses_grant_research_without_a_side) {
  CHECK(has(loadProblem(startEvent(R"([{"grantResearch":"tools"}])")), "\"side\""));
}
TEST(story_loader_refuses_grant_research_with_other_keys) {
  CHECK(has(loadProblem(startEvent(R"([{"grantResearch":"tools","side":"blue","cost":1}])")), "unknown key \"cost\""));
}
TEST(story_loader_says_when_the_game_has_no_research) {
  fixture::World plain({"...."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false}});
  story::StoryWorld w({"...."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false}}, {"a", "b"});
  std::string err = w.load(startEvent(R"([{"grantResearch":"tools","side":"blue"}])"));
  CHECK(has(err, "unknown technology") && has(err, "no data/research.json"));
}
