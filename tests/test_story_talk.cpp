// The Talk action (docs/EVENTS.md): when it is offered, what it does, endTurn, repeat; the
// computer's side never uses it.
#include "core/ai/ai.h"
#include "story_fixture.h"

using namespace ote;
using namespace story;

namespace {

// eirik (0,1) and lyra (3,1) are blue, gorm (8,1) is red.
struct TalkWorld : StoryWorld {
  TalkWorld()
      : StoryWorld({".........", ".........", "........."},
                   {{"soldier", 0, 0, 1, false}, {"soldier", 0, 3, 1, false}, {"soldier", 1, 8, 1, false}},
                   {"eirik", "lyra", "gorm"}) {}
  std::vector<TalkOption> options(int unit, Cell from) const {
    std::vector<TalkOption> out;
    battle.talkOptions(unit, from, out);
    return out;
  }
};

const char* kEvents = R"([
  {"id":"chat","when":{"talk":"eirik","to":"lyra","label":"Chat"},"do":[
     {"say":"Lyra, a word.","speaker":"eirik"},{"setSwitch":"saved","to":true}]},
  {"id":"leave","when":{"talk":"eirik","to":"lyra","label":"Go on ahead"},"repeat":true,
   "if":[{"switch":"saved"}],"do":[{"say":"Go on.","speaker":"eirik"},{"endTurn":true}]}])";

}  // namespace

TEST(talk_is_offered_next_to_the_named_unit_only) {
  TalkWorld w;
  CHECK(w.play(kEvents));
  CHECK(w.options(0, w.at(0, 1)).empty());                 // lyra is three squares away
  std::vector<TalkOption> o = w.options(0, w.at(2, 1));    // as if eirik stood next to her
  CHECK_EQ(o.size(), (size_t)1);                           // the second event needs the switch
  if (o.size() != 1) return;
  CHECK(o[0].target == 1 && o[0].event == 0);
  CHECK_STR(w.battle.storyText(o[0].label), "Chat");
  CHECK(w.options(1, w.at(2, 1)).empty());                 // only eirik can start this talk
  CHECK(w.options(2, w.at(2, 1)).empty());                 // gorm: not his turn
  std::string why;
  CHECK(!w.battle.apply(Action::talk(0, 1), w.events, &why));
  CHECK(why.find("nobody") != std::string::npos);
}

TEST(talk_runs_the_event_and_does_not_end_the_turn) {
  TalkWorld w;
  CHECK(w.play(kEvents));
  CHECK(w.act(Action::move(0, w.at(2, 1))));
  CHECK(w.act(Action::talk(0, 1)));
  CHECK(w.fired("chat") && w.sw("saved"));
  CHECK(w.saidLine("Lyra, a word."));
  CHECK(!w.battle.units[0].done);                          // free, like the item actions
  CHECK(w.act(Action::attack(0, 1, 0)) == false);          // (lyra is an ally) ... but the unit still acts:
  CHECK(w.act(Action::wait(0)));
}

TEST(talk_with_end_turn_ends_the_units_turn_and_repeats) {
  TalkWorld w;
  CHECK(w.play(kEvents));
  CHECK(w.act(Action::move(0, w.at(2, 1))));
  CHECK(w.act(Action::talk(0, 1)));                        // "chat" runs once ...
  std::vector<TalkOption> o = w.options(0, w.battle.units[0].cell);
  CHECK(o.size() == 1 && w.battle.storyText(o[0].label) == "Go on ahead");  // ... and "leave" is now offered
  CHECK(!w.act(Action::talk(0, 1, 0)));                    // the first event cannot run again
  CHECK(w.act(Action::talk(0, 1, 1)));
  CHECK(w.battle.units[0].done && w.battle.units[0].moved);
  CHECK(!w.act(Action::talk(0, 1)));                       // the turn is over for him
  CHECK(w.options(0, w.battle.units[0].cell).empty());
  CHECK(w.act(Action::endTurn()) && w.act(Action::endTurn()));
  CHECK_EQ(w.options(0, w.battle.units[0].cell).size(), (size_t)1);  // "leave" repeats, "chat" does not
}

TEST(talk_needs_both_units_alive_and_the_condition_to_hold) {
  TalkWorld w;
  CHECK(w.play(R"([{"id":"gated","when":{"talk":"eirik","to":"lyra"},"if":[{"switch":"alarm"}],"do":[]}])"));
  CHECK(w.act(Action::move(0, w.at(2, 1))));
  CHECK(w.options(0, w.battle.units[0].cell).empty());     // the condition does not hold yet
  w.battle.storyState.switches[1] = 1;
  CHECK_EQ(w.options(0, w.battle.units[0].cell).size(), (size_t)1);
  w.battle.units[1].alive = false;                         // lyra falls
  CHECK(w.options(0, w.battle.units[0].cell).empty());
  CHECK(!w.act(Action::talk(0, 1)));
}

TEST(talk_label_defaults_to_talk) {
  TalkWorld w;
  CHECK(w.play(R"([{"id":"t","when":{"talk":"eirik","to":"lyra"},"do":[]}])"));
  std::vector<TalkOption> o = w.options(0, w.at(2, 1));
  CHECK(o.size() == 1 && w.battle.storyText(o[0].label) == "Talk");
}

TEST(talk_is_never_used_by_the_computer) {
  TalkWorld w;
  w.map.units[1].x = 7;  // lyra next to gorm instead; gorm (red) may "talk to" lyra
  CHECK(w.play(R"([{"id":"t","when":{"talk":"gorm","to":"lyra"},"do":[{"say":"hm"}]}])"));
  CHECK(w.act(Action::endTurn()));                         // red's turn
  CHECK(!w.options(2, w.battle.units[2].cell).empty());    // offered to a human controlling red ...
  int guard = 0;
  while (w.battle.activeSide == 1 && !w.battle.over && guard++ < 50) aiTakeStep(w.battle, w.events);
  CHECK(!w.fired("t"));                                    // ... but the AI plays on without it
}
