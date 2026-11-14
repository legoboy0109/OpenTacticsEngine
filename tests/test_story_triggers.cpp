// Story events in battle (docs/EVENTS.md): every trigger, every condition, repeat, the queue and
// the chain limit.
#include "story_fixture.h"

using namespace ote;
using namespace story;

namespace {

// 9 x 3 plain field. eirik (blue soldier) at 0,1 and gorm (red soldier) at 8,1 unless a test places units itself.
std::vector<std::string> field() { return {".........", ".........", "........."}; }

}  // namespace

TEST(story_battle_start_runs_before_the_first_turn) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play(R"([{"id":"intro","when":{"battle":"start"},"do":[{"say":"Begin.","speaker":"eirik"}]}])"));
  int d = find(w.events, EventType::Dialogue), t = find(w.events, EventType::TurnStarted);
  CHECK(d >= 0 && t >= 0 && d < t);
  CHECK(w.fired("intro"));
  CHECK(w.saidLine("Soldier: Begin."));
}

TEST(story_turn_trigger_names_the_round_and_the_side) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play(R"([
    {"id":"first","when":{"turn":1},"do":[{"say":"first turn"}]},
    {"id":"ambush","when":{"turn":2,"side":"red"},"do":[{"say":"ambush"}]},
    {"id":"tick","when":{"turn":"every"},"do":[{"addVariable":"kills","by":1}]}])"));
  CHECK(w.fired("first"));                                 // round 1, the first side's turn: right at the start
  CHECK_EQ(w.var("kills"), 1);                             // "every" runs each round (blue's turn) without "repeat"
  CHECK(w.act(Action::endTurn()));                         // red, round 1
  CHECK(w.act(Action::endTurn()));                         // blue, round 2
  CHECK(!w.fired("ambush"));
  CHECK_EQ(w.var("kills"), 2);
  CHECK(w.act(Action::endTurn()));                         // red, round 2
  CHECK(w.fired("ambush"));
  CHECK_EQ(w.var("kills"), 2);                             // red's turn is not "every" round's side
  CHECK_EQ(w.said().size(), (size_t)2);
}

TEST(story_reach_fires_when_a_unit_ends_a_move_inside_the_area) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}, {"archer", 0, 0, 2, false}},
               {"eirik", "gorm", "lyra"});
  CHECK(w.play(R"([
    {"id":"r_eirik","when":{"reach":{"x":3,"y":1,"w":2,"h":1},"by":{"unit":"eirik"}},"do":[{"say":"eirik arrives"}]},
    {"id":"r_red","when":{"reach":{"x":3,"y":1,"w":3,"h":1},"by":{"side":"red"}},"do":[{"say":"red arrives"}]},
    {"id":"r_lyra","when":{"reach":{"x":3,"y":1},"by":"lyra"},"do":[{"say":"lyra arrives"}]},
    {"id":"r_pass","when":{"reach":{"x":1,"y":1},"by":{"unit":"eirik"}},"do":[{"say":"passing"}]},
    {"id":"r_any","when":{"reach":{"x":5,"y":0,"w":1,"h":3}},"do":[{"say":"someone"}]}])"));
  CHECK(w.act(Action::move(0, w.at(3, 1))));
  CHECK(w.fired("r_eirik"));
  CHECK(!w.fired("r_red") && !w.fired("r_lyra") && !w.fired("r_any"));
  CHECK(!w.fired("r_pass"));                               // walking through is not stopping
  // A unit of the computer's side triggers events just the same.
  CHECK(w.act(Action::endTurn()));
  CHECK(w.act(Action::move(1, w.at(5, 1))));
  CHECK(w.fired("r_red") && w.fired("r_any"));
  CHECK(!w.fired("r_lyra"));
}

TEST(story_a_failed_condition_does_not_use_up_the_event) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play(R"([{"id":"gate","when":{"reach":{"x":3,"y":1,"w":2,"h":1},"by":"eirik"},
                    "if":[{"switch":"alarm","is":true}],"do":[{"say":"open"}]}])"));
  CHECK(w.act(Action::move(0, w.at(3, 1))));
  CHECK(!w.fired("gate"));                                 // the condition did not hold ...
  w.battle.storyState.switches[1] = 1;                     // ... until the alarm rings
  CHECK(w.act(Action::endTurn()) && w.act(Action::endTurn()));
  CHECK(w.act(Action::move(0, w.at(4, 1))));
  CHECK(w.fired("gate"));
}

TEST(story_events_run_once_unless_they_repeat) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play(R"([
    {"id":"once","when":{"reach":{"x":3,"y":1},"by":"eirik"},"do":[{"addVariable":"gold","by":1}]},
    {"id":"again","when":{"reach":{"x":3,"y":1},"by":"eirik"},"repeat":true,"do":[{"addVariable":"kills","by":1}]}])"));
  for (int x : {3, 4, 3, 4, 3}) {  // in, out, in, out, in
    CHECK(w.act(Action::move(0, w.at(x, 1))));
    CHECK(w.act(Action::endTurn()) && w.act(Action::endTurn()));
  }
  CHECK_EQ(w.var("gold"), 1);
  CHECK_EQ(w.var("kills"), 3);
}

TEST(story_defeated_trigger_with_and_without_by) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 1, 1, false}, {"soldier", 1, 8, 1, false},
                         {"archer", 0, 0, 2, false}},
               {"eirik", "gorm", "", "lyra"});
  CHECK(w.play(R"([
    {"id":"d1","when":{"defeated":{"unit":"gorm"},"by":{"unit":"eirik"}},"do":[{"say":"d1"}]},
    {"id":"d2","when":{"defeated":{"side":"red"}},"do":[{"say":"d2"}]},
    {"id":"d3","when":{"defeated":{"unit":"gorm"},"by":{"unit":"lyra"}},"do":[{"say":"d3"}]},
    {"id":"d4","when":{"defeated":{"type":"archer"}},"do":[{"say":"d4"}]}])"));
  w.battle.units[1].hp = 1;
  CHECK(w.act(Action::attack(0, 1, 0)));
  CHECK(w.fired("d1") && w.fired("d2"));
  CHECK(!w.fired("d3") && !w.fired("d4"));
  // The line comes right after the blow and the death, in the event stream.
  int kill = find(w.events, EventType::UnitDied), line = find(w.events, EventType::Dialogue);
  CHECK(kill >= 0 && line > kill);
}

TEST(story_defeated_when_a_side_is_wiped_out_by_the_rules_has_no_killer) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 1, 1, true}, {"soldier", 1, 8, 1, false}},
               {"eirik", "boss", "grunt"});
  w.db.rules.defeatLeaderDead = true;
  CHECK(w.play(R"([
    {"id":"any","when":{"defeated":{"unit":"grunt"}},"do":[{"say":"grunt fell"}]},
    {"id":"by_blue","when":{"defeated":{"unit":"grunt"},"by":{"side":"blue"}},"do":[{"say":"killed by blue"}]}])"));
  w.battle.units[1].hp = 1;
  CHECK(w.act(Action::attack(0, 1, 0)));                   // the leader falls: the whole red side leaves the field
  CHECK(w.battle.units[2].alive == false);
  CHECK(w.fired("any"));
  CHECK(!w.fired("by_blue"));
}

TEST(story_fight_runs_just_before_the_fight_whichever_side_the_unit_is_on) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 1, 1, false}, {"tank", 1, 8, 1, false}},
               {"eirik", "gorm", "tiger"});
  CHECK(w.play(R"([
    {"id":"f1","when":{"fight":{"unit":"eirik"},"with":{"type":"soldier"}},"do":[{"say":"f1"}]},
    {"id":"f2","when":{"fight":{"unit":"gorm"}},"do":[{"say":"f2"}]},
    {"id":"f3","when":{"fight":{"unit":"eirik"},"with":{"type":"tank"}},"do":[{"say":"f3"}]},
    {"id":"f4","when":{"fight":{"side":"blue"},"with":{"side":"blue"}},"do":[{"say":"f4"}]}])"));
  CHECK(w.act(Action::attack(0, 1, 0)));
  CHECK(w.fired("f1") && w.fired("f2"));                   // gorm is the one attacked: still "fighting"
  CHECK(!w.fired("f3") && !w.fired("f4"));
  int line = find(w.events, EventType::Dialogue), hit = find(w.events, EventType::Attack);
  CHECK(line >= 0 && hit > line);
}

TEST(story_a_fight_event_can_call_the_fight_off) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 1, 1, false}, {"soldier", 1, 8, 1, false}},
               {"eirik", "gorm", "grunt"});
  CHECK(w.play(R"([{"id":"flee","when":{"fight":{"unit":"gorm"}},"do":[{"removeUnit":{"unit":"gorm"}}]}])"));
  CHECK(w.act(Action::attack(0, 1, 0)));
  CHECK_EQ(w.count(EventType::Attack), 0);
  CHECK(!w.battle.units[1].alive);
  CHECK(w.battle.units[0].done);                           // the attack was used up
}

TEST(story_battle_end_triggers_follow_the_human_team) {
  {
    StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 1, 1, false}}, {"eirik", "gorm"});
    CHECK(w.play(R"([
      {"id":"w","when":{"battle":"won"},"do":[{"say":"won"}]},
      {"id":"l","when":{"battle":"lost"},"do":[{"say":"lost"}]},
      {"id":"e","when":{"battle":"end"},"if":[{"dead":"gorm"}],"do":[{"say":"end"}]}])"));
    w.battle.units[1].hp = 1;
    CHECK(w.act(Action::attack(0, 1, 0)));
    CHECK(w.battle.over && w.battle.winnerTeam == 0);
    CHECK(w.fired("w") && w.fired("e") && !w.fired("l"));
    CHECK(find(w.events, EventType::BattleEnded) < find(w.events, EventType::Dialogue));  // epilogue after the result
  }
  {
    StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 1, 1, false}}, {"eirik", "gorm"});
    CHECK(w.play(R"([
      {"id":"w","when":{"battle":"won"},"do":[{"say":"won"}]},
      {"id":"l","when":{"battle":"lost"},"do":[{"say":"lost"}]},
      {"id":"e","when":{"battle":"end"},"do":[{"say":"end"}]}])"));
    w.battle.units[0].hp = 1;
    CHECK(w.act(Action::endTurn()));
    CHECK(w.act(Action::attack(1, 0, 0)));
    CHECK(w.battle.over && w.battle.winnerTeam == 1);
    CHECK(!w.fired("w") && w.fired("l") && w.fired("e"));
  }
}

TEST(story_conditions) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  fixture::addItems(w);
  fixture::carry(w, 0, {fixture::items::Potion});
  // The first event sets things up; later events are judged when they are about to run, so they see it.
  CHECK(w.play(R"([
    {"id":"setup","when":{"battle":"start"},"do":[{"setSwitch":"saved","to":true},{"setVariable":"gold","to":5}]},
    {"id":"sw_on","when":{"battle":"start"},"if":[{"switch":"saved","is":true}],"do":[]},
    {"id":"sw_off","when":{"battle":"start"},"if":[{"switch":"alarm","is":false}],"do":[]},
    {"id":"sw_default","when":{"battle":"start"},"if":[{"switch":"alarm"}],"do":[]},
    {"id":"var_ok","when":{"battle":"start"},"if":[{"variable":"gold","op":">=","value":5},{"variable":"gold","op":"!=","value":4},
       {"variable":"gold","op":"<","value":6},{"variable":"gold","op":"<=","value":5},{"variable":"gold","op":">","value":4},
       {"variable":"gold","op":"=","value":5},{"variable":"gold","value":5}],"do":[]},
    {"id":"var_no","when":{"battle":"start"},"if":[{"variable":"gold","op":">","value":5}],"do":[]},
    {"id":"alive_ok","when":{"battle":"start"},"if":[{"alive":"eirik"},{"alive":{"side":"red"}}],"do":[]},
    {"id":"dead_no","when":{"battle":"start"},"if":[{"dead":"gorm"}],"do":[]},
    {"id":"round_ok","when":{"battle":"start"},"if":[{"round":{"op":"=","value":1}}],"do":[]},
    {"id":"round_no","when":{"battle":"start"},"if":[{"round":{"op":">","value":1}}],"do":[]},
    {"id":"carry_ok","when":{"battle":"start"},"if":[{"carries":"potion","unit":{"unit":"eirik"}}],"do":[]},
    {"id":"carry_any","when":{"battle":"start"},"if":[{"carries":"potion"}],"do":[]},
    {"id":"carry_no","when":{"battle":"start"},"if":[{"carries":"potion","unit":{"side":"red"}}],"do":[]}])"));
  for (const char* id : {"setup", "sw_on", "sw_off", "var_ok", "alive_ok", "round_ok", "carry_ok", "carry_any"})
    CHECK(w.fired(id));
  for (const char* id : {"sw_default", "var_no", "dead_no", "round_no", "carry_no"}) CHECK(!w.fired(id));
}

TEST(story_events_that_fire_while_one_runs_wait_their_turn) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 1, 1, false}, {"soldier", 1, 8, 1, false}},
               {"eirik", "gorm", "grunt"});
  // The first event ends the battle in its middle; the events for that wait until it has finished.
  CHECK(w.play(R"([
    {"id":"start","when":{"battle":"start"},"do":[{"say":"one"},{"win":"blue"},{"say":"two"}]},
    {"id":"after","when":{"battle":"won"},"do":[{"say":"three"}]}])"));
  CHECK(w.battle.over);
  std::vector<std::string> s = w.said();
  CHECK(s.size() == 3 && s[0] == ": one" && s[1] == ": two" && s[2] == ": three");
}

TEST(story_a_chain_of_events_is_cut_after_a_hundred) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  // Moving eirik into one square sets off the event that moves him into the other, and back.
  CHECK(w.play(R"([
    {"id":"ping","when":{"reach":{"x":5,"y":1},"by":"eirik"},"repeat":true,"do":[{"moveUnit":"eirik","x":6,"y":1}]},
    {"id":"pong","when":{"reach":{"x":6,"y":1},"by":"eirik"},"repeat":true,"do":[{"moveUnit":"eirik","x":5,"y":1}]}])"));
  w.battle.units[0].stats[3] = 6;                          // enough move to get there
  CHECK(w.act(Action::move(0, w.at(5, 1))));
  CHECK_EQ(w.count(EventType::UnitRelocated), 100);        // exactly a hundred events ran, then the chain was cut
  CHECK(w.battle.units[0].alive && w.battle.occupant[(size_t)w.battle.units[0].cell] == 0);
  CHECK(w.act(Action::wait(0)));                           // the battle is intact and goes on
}
