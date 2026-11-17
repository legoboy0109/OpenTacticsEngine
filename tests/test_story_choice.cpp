// Dialogue choices in a battle (docs/EVENTS.md "Dialogue choices"): the loader's refusals, the first answer taken when
// nobody is asked, a question that waits (every answer, a nested question, a question in the middle of an action),
// hidden answers, and that the choice is saved with the battle.
#include <algorithm>

#include "save_helpers.h"
#include "story_fixture.h"

using namespace ote;
using namespace story;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

StoryWorld world() {
  return StoryWorld({".........", ".........", "........."}, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}},
                    {"eirik", "gorm"});
}

std::string startEvents(const std::string& commands) {
  return "[{\"id\":\"e\",\"when\":{\"battle\":\"start\"},\"do\":" + commands + "}]";
}
std::string commandError(const std::string& command) {
  StoryWorld w = world();
  return w.load(startEvents("[" + command + "]"));
}

// A battle in which questions wait for a person: init, ask on, start.
bool beginAsking(StoryWorld& w, uint32_t seed = 1) {
  std::string err;
  if (!w.formulas.init(w.db, err) || !w.battle.init(w.db, w.map, w.formulas, seed, err)) {
    std::printf("  init: %s\n", err.c_str());
    return false;
  }
  w.battle.setAskChoices(true);
  w.battle.start(w.events);
  return true;
}

bool answer(StoryWorld& w, int pick) { return w.battle.answerChoice(pick, w.events); }

const char* kGate = R"([{"say":"Intro"},
  {"choice":"Open the gate?","speaker":"eirik","store":"gold","answers":[
    {"text":"Open","do":[{"say":"It opens."},{"setSwitch":"saved","to":true}]},
    {"text":"Hold","do":[{"say":"It holds."},{"setSwitch":"alarm","to":true}]},
    {"text":"Ask Gorm","do":[{"addVariable":"kills","by":5}]}]},
  {"say":"Afterwards"}])";

std::string lastSaid(const StoryWorld& w) {
  std::vector<std::string> s = w.said();
  return s.empty() ? "" : s.back();
}

}  // namespace

// ---- the loader ----

TEST(choice_loader_reads_a_question) {
  StoryWorld w = world();
  CHECK_STR(w.load(startEvents(kGate)), "");
  const Command& c = w.map.story->events[0].commands[1];
  CHECK(c.kind == Command::Kind::Choice);
  CHECK_EQ((int)c.answers.size(), 3);
  CHECK_EQ(c.index, w.db.findVariable("gold"));
  CHECK_STR(w.map.story->texts[(size_t)c.answers[1].text], "Hold");
  CHECK_EQ((int)c.answers[0].commands.size(), 2);
  CHECK(w.map.story->hasChoice);
  // A map without a choice does not ask for the question machinery.
  CHECK_STR(w.load(startEvents(R"([{"say":"x"}])")), "");
  CHECK(!w.map.story->hasChoice);
}

TEST(choice_loader_refuses_fewer_than_two_answers) {
  std::string e = commandError(R"({"choice":"Q?","answers":[{"text":"Only"}]})");
  CHECK(has(e, "command 1 (choice)") && has(e, "has 1 answer; a question needs two to four"));
}
TEST(choice_loader_refuses_more_than_four_answers) {
  std::string e = commandError(R"({"choice":"Q?","answers":[{"text":"a"},{"text":"b"},{"text":"c"},{"text":"d"},{"text":"e"}]})");
  CHECK(has(e, "has 5 answers; a question needs two to four"));
}
TEST(choice_loader_accepts_two_and_four_answers) {
  CHECK_STR(commandError(R"({"choice":"Q?","answers":[{"text":"a"},{"text":"b"}]})"), "");
  CHECK_STR(commandError(R"({"choice":"Q?","answers":[{"text":"a"},{"text":"b"},{"text":"c"},{"text":"d"}]})"), "");
}
TEST(choice_loader_refuses_empty_texts) {
  CHECK(has(commandError(R"({"choice":"","answers":[{"text":"a"},{"text":"b"}]})"), "must be the question to ask"));
  CHECK(has(commandError(R"({"choice":"Q?","answers":[{"text":"a"},{"text":""}]})"), "answer 2: \"text\" must be the words of the answer"));
  CHECK(has(commandError(R"({"choice":"Q?","answers":[{"text":"a"},{"do":[]}]})"), "answer 2: \"text\" must be the words"));
}
TEST(choice_loader_refuses_two_answers_that_say_the_same) {
  CHECK(has(commandError(R"({"choice":"Q?","answers":[{"text":"Yes"},{"text":"Yes"}]})"), "answer 2: says the same as an earlier answer"));
}
TEST(choice_loader_refuses_an_unknown_command_inside_an_answer) {
  std::string e = commandError(R"({"choice":"Q?","answers":[{"text":"a"},{"text":"b","do":[{"dance":1}]}]})");
  CHECK(has(e, "command 1 (choice): answer 2: \"do\": command 1") && has(e, "is not a command"));
}
TEST(choice_loader_refuses_a_bad_command_inside_an_answer) {
  std::string e = commandError(R"({"choice":"Q?","answers":[{"text":"a","do":[{"setSwitch":"nope","to":true}]},{"text":"b"}]})");
  CHECK(has(e, "answer 1: \"do\": command 1 (setSwitch)") && has(e, "unknown switch \"nope\""));
}
TEST(choice_loader_refuses_unknown_keys_variables_and_speakers) {
  CHECK(has(commandError(R"({"choice":"Q?","colour":1,"answers":[{"text":"a"},{"text":"b"}]})"), "unknown key \"colour\""));
  CHECK(has(commandError(R"({"choice":"Q?","answers":[{"text":"a","then":[]},{"text":"b"}]})"), "answer 1: unknown key \"then\""));
  CHECK(has(commandError(R"({"choice":"Q?","store":"nope","answers":[{"text":"a"},{"text":"b"}]})"), "\"store\" names unknown variable \"nope\""));
  CHECK(has(commandError(R"({"choice":"Q?","speaker":"nobody","answers":[{"text":"a"},{"text":"b"}]})"), "unknown unit \"nobody\""));
  CHECK(has(commandError(R"({"choice":"Q?","answers":5})"), "\"answers\" must be a list"));
  CHECK(has(commandError(R"({"choice":"Q?"})"), "missing required \"answers\""));
  CHECK(has(commandError(R"({"choice":"Q?","answers":[{"text":"a","if":[{"switch":"nope"}]},{"text":"b"}]})"), "unknown switch"));
}
TEST(choice_loader_refuses_two_questions_writing_one_variable) {
  std::string e = commandError(R"({"choice":"A?","store":"gold","answers":[{"text":"a"},{"text":"b"}]},
                                  {"choice":"B?","store":"gold","answers":[{"text":"a"},{"text":"b"}]})");
  CHECK(has(e, "command 2 (choice)") && has(e, "which another choice of this map already writes"));
  // Different variables, or no variable at all, are fine.
  CHECK_STR(commandError(R"({"choice":"A?","store":"gold","answers":[{"text":"a"},{"text":"b"}]},
                           {"choice":"B?","store":"kills","answers":[{"text":"a"},{"text":"b"}]},
                           {"choice":"C?","answers":[{"text":"a"},{"text":"b"}]},{"choice":"D?","answers":[{"text":"a"},{"text":"b"}]})"), "");
}
TEST(choice_loader_lets_an_answer_add_a_unit_that_a_later_event_names) {
  StoryWorld w = world();
  CHECK_STR(w.load(R"([{"id":"a","when":{"battle":"start"},"do":[{"choice":"Q?","answers":[
      {"text":"y","do":[{"addUnit":{"type":"soldier","side":"red","x":5,"y":0,"id":"guard"}}]},{"text":"n"}]}]},
    {"id":"b","when":{"turn":2},"do":[{"say":"Guard!","speaker":"guard"}]}])"), "");
}

// ---- nobody to ask: the first answer ----

TEST(choice_without_asking_takes_the_first_offered_answer) {
  StoryWorld w = world();
  CHECK(w.play(startEvents(kGate)));
  CHECK(w.sw("saved") && !w.sw("alarm"));
  CHECK_EQ(w.var("gold"), 1);
  CHECK_EQ(w.count(EventType::Choice), 0);
  std::vector<std::string> said = w.said();
  CHECK_EQ((int)said.size(), 4);  // Intro, the question, It opens., Afterwards
  CHECK(said.size() == 4 && said[1] == "Soldier: Open the gate?" && said[2] == ": It opens." && said[3] == ": Afterwards");
  CHECK(!w.battle.choiceWaiting());
}

TEST(choice_asking_is_off_unless_the_player_turns_it_on) {
  StoryWorld w = world();
  CHECK_STR(w.load(startEvents(kGate)), "");
  std::string err;
  CHECK(w.formulas.init(w.db, err) && w.battle.init(w.db, w.map, w.formulas, 1, err));
  w.battle.start(w.events);
  CHECK(!w.battle.choiceWaiting() && w.sw("saved"));
}

// ---- a question that waits ----

TEST(choice_waits_and_every_answer_runs_its_own_commands) {
  struct Path { int pick; bool saved, alarm; int gold, kills; const char* said; };
  const Path paths[] = {{0, true, false, 1, 0, ": It opens."}, {1, false, true, 2, 0, ": It holds."}, {2, false, false, 3, 5, ": Afterwards"}};
  for (const Path& p : paths) {
    StoryWorld w = world();
    CHECK_STR(w.load(startEvents(kGate)), "");
    CHECK(beginAsking(w));
    CHECK(w.battle.choiceWaiting());
    // The entries stop at the question: the first turn has not begun.
    CHECK_EQ(w.count(EventType::TurnStarted), 0);
    const Event* q = w.last(EventType::Choice);
    CHECK(q && &w.events.back() == q);
    if (!q) return;
    CHECK_STR(w.battle.storyText(q->b), "Open the gate?");
    CHECK(q->a == w.unit("eirik") && (int)q->data.size() == 3);
    CHECK_STR(w.battle.storyText(q->data[2]), "Ask Gorm");
    CHECK_EQ(w.var("gold"), 0);  // nothing of the answer has happened yet
    CHECK(!w.sw("saved"));
    CHECK(!w.act(Action::endTurn()));  // nothing else can be done meanwhile
    std::string before = savetest::stateText(w.battle);
    CHECK(!answer(w, 3) && !answer(w, -1));  // out of range: nothing changes
    CHECK_STR(savetest::stateText(w.battle), before);
    CHECK(w.battle.choiceWaiting());

    size_t at = w.events.size();
    CHECK(answer(w, p.pick));
    CHECK(!w.battle.choiceWaiting());
    CHECK_EQ(w.sw("saved") ? 1 : 0, p.saved ? 1 : 0);
    CHECK_EQ(w.sw("alarm") ? 1 : 0, p.alarm ? 1 : 0);
    CHECK_EQ(w.var("gold"), p.gold);
    CHECK_EQ(w.var("kills"), p.kills);
    CHECK_EQ(w.count(EventType::Choice), 1);  // the question is not repeated
    CHECK(w.events.size() > at && lastSaid(w) == ": Afterwards" && w.saidLine(p.said));
    CHECK_EQ(w.count(EventType::TurnStarted), 1);  // the start went on to the first turn
    CHECK(w.act(Action::endTurn()));
  }
}

TEST(choice_answered_run_is_the_same_as_writing_the_answer_out) {
  // Asking and answering "Hold" must leave exactly the battle that the plain commands of that answer leave.
  StoryWorld a = world(), b = world();
  CHECK_STR(a.load(startEvents(kGate)), "");
  CHECK(beginAsking(a) && answer(a, 1));
  CHECK(b.play(startEvents(R"([{"say":"Intro"},{"setVariable":"gold","to":2},{"say":"It holds."},{"setSwitch":"alarm","to":true},{"say":"Afterwards"}])")));
  CHECK_STR(savetest::stateText(a.battle), savetest::stateText(b.battle));
}

TEST(choice_in_the_middle_of_an_action_finishes_the_action_after_the_answer) {
  const char* events = R"([{"id":"gate","when":{"reach":{"x":3,"y":1},"by":"eirik"},"do":[
      {"say":"Halt!","speakerName":"Guard"},
      {"choice":"Pay the toll?","store":"gold","answers":[
         {"text":"Pay","do":[{"say":"Paid."}]},
         {"text":"Fight","do":[{"setSwitch":"alarm","to":true},{"addUnit":{"type":"soldier","side":"red","x":3,"y":2,"id":"guard"}}]}]},
      {"setVariable":"kills","to":9}]}])";
  StoryWorld a = world(), b = world();
  CHECK_STR(a.load(events), "");
  CHECK(beginAsking(a));
  CHECK(!a.battle.choiceWaiting());  // nothing at the start
  CHECK(a.act(Action::move(a.unit("eirik"), a.at(3, 1))));
  CHECK(a.battle.choiceWaiting());
  // The entries so far: the walk, the line, the question. The unit already stands on the square.
  const Event* q = a.last(EventType::Choice);
  CHECK(q && &a.events.back() == q && a.count(EventType::UnitMoved) == 1);
  CHECK(a.battle.units[(size_t)a.unit("eirik")].cell == a.at(3, 1));
  CHECK_EQ(a.var("kills"), 0);
  size_t at = a.events.size();
  CHECK(answer(a, 1));
  CHECK(a.var("kills") == 9 && a.var("gold") == 2 && a.sw("alarm") && a.unit("guard") >= 0);
  CHECK_EQ(a.count(EventType::UnitMoved), 1);  // the move is not repeated in what the caller sees
  CHECK(a.events.size() > at && a.events[at].type == EventType::UnitAdded);
  // The same battle without a question in the way.
  CHECK(b.play(R"([{"id":"gate","when":{"reach":{"x":3,"y":1},"by":"eirik"},"do":[
      {"setVariable":"gold","to":2},{"setSwitch":"alarm","to":true},{"addUnit":{"type":"soldier","side":"red","x":3,"y":2,"id":"guard"}},
      {"setVariable":"kills","to":9}]}])"));
  CHECK(b.act(Action::move(b.unit("eirik"), b.at(3, 1))));
  CHECK_STR(savetest::stateText(a.battle), savetest::stateText(b.battle));
  // The turn goes on normally afterwards.
  CHECK(a.act(Action::wait(a.unit("eirik"))) && a.act(Action::endTurn()));
}

TEST(choice_in_a_nested_question_waits_twice) {
  const char* commands = R"([{"choice":"First?","store":"gold","answers":[
      {"text":"A","do":[{"choice":"Second?","store":"kills","answers":[
          {"text":"A1","do":[{"say":"a1"}]},{"text":"A2","do":[{"say":"a2"},{"setSwitch":"saved","to":true}]}]},{"say":"end of A"}]},
      {"text":"B","do":[{"say":"b"}]}]},{"say":"done"}])";
  StoryWorld w = world();
  CHECK_STR(w.load(startEvents(commands)), "");
  CHECK(beginAsking(w));
  CHECK(w.battle.choiceWaiting() && w.count(EventType::Choice) == 1);
  CHECK(answer(w, 0));
  CHECK(w.battle.choiceWaiting() && w.count(EventType::Choice) == 2);  // the second question, not the first again
  CHECK_STR(w.battle.storyText(w.last(EventType::Choice)->b), "Second?");
  CHECK(!w.sw("saved") && !w.saidLine("done"));
  CHECK(answer(w, 1));
  CHECK(!w.battle.choiceWaiting());
  CHECK(w.sw("saved") && w.var("gold") == 1 && w.var("kills") == 2);
  CHECK(w.saidLine("a2") && w.saidLine("end of A") && w.saidLine("done") && !w.saidLine("a1"));
  CHECK_EQ(w.count(EventType::Choice), 2);
}

TEST(choice_two_questions_in_a_row_in_one_event) {
  StoryWorld w = world();
  CHECK_STR(w.load(startEvents(R"([{"choice":"One?","store":"gold","answers":[{"text":"x"},{"text":"y"}]},
                                  {"choice":"Two?","store":"kills","answers":[{"text":"x"},{"text":"y"}]},{"say":"end"}])")), "");
  CHECK(beginAsking(w));
  CHECK(answer(w, 1) && w.battle.choiceWaiting());
  CHECK(answer(w, 0) && !w.battle.choiceWaiting());
  CHECK(w.var("gold") == 2 && w.var("kills") == 1 && w.saidLine("end"));
}

TEST(choice_in_a_talk_event_that_ends_the_turn) {
  StoryWorld w({".........", ".........", "........."},
               {{"soldier", 0, 0, 1, false}, {"soldier", 0, 1, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK_STR(w.load(R"([{"id":"talk","when":{"talk":"eirik","to":"gorm"},"do":[
      {"choice":"Join us?","speaker":"gorm","store":"gold","answers":[{"text":"Yes","do":[{"setSwitch":"saved","to":true}]},{"text":"No"}]},
      {"endTurn":true}]}])"), "");
  CHECK(beginAsking(w));
  const int eirik = w.unit("eirik"), gorm = w.unit("gorm");
  CHECK(w.act(Action::talk(eirik, gorm)));
  CHECK(w.battle.choiceWaiting() && !w.battle.units[(size_t)eirik].done);  // the turn is not over until the answer is in
  CHECK(answer(w, 0));
  CHECK(w.sw("saved") && w.var("gold") == 1 && w.battle.units[(size_t)eirik].done);
}

// ---- hidden answers ----

TEST(choice_hides_answers_whose_condition_fails) {
  const char* commands = R"([{"choice":"Which way?","store":"gold","answers":[
      {"text":"Left","if":[{"switch":"alarm"}]},
      {"text":"Straight"},
      {"text":"Right","if":[{"switch":"saved"}],"do":[{"say":"right"}]},
      {"text":"Back","do":[{"say":"back"}]}]}])";
  StoryWorld w = world();
  CHECK_STR(w.load(startEvents(commands)), "");
  CHECK(beginAsking(w));
  const Event* q = w.last(EventType::Choice);
  CHECK(q && q->data.size() == 2);  // only Straight and Back are offered
  CHECK_STR(w.battle.storyText(q->data[0]), "Straight");
  CHECK_STR(w.battle.storyText(q->data[1]), "Back");
  CHECK(answer(w, 1));  // position 1 among the offered = "Back", number 4 among the written
  CHECK_EQ(w.var("gold"), 4);
  CHECK(w.saidLine("back"));
}

TEST(choice_with_one_offered_answer_takes_it_without_asking) {
  const char* commands = R"([{"choice":"Which way?","store":"gold","answers":[
      {"text":"Left","if":[{"switch":"alarm"}]},{"text":"Right","if":[{"switch":"saved"}]},{"text":"Straight","do":[{"say":"straight"}]}]},
      {"say":"end"}])";
  StoryWorld w = world();
  CHECK_STR(w.load(startEvents(commands)), "");
  CHECK(beginAsking(w));
  CHECK(!w.battle.choiceWaiting() && w.count(EventType::Choice) == 0);
  CHECK(w.var("gold") == 3 && w.saidLine("straight") && w.saidLine("end"));
  CHECK(!w.saidLine("Which way?"));  // nothing was asked
}

TEST(choice_with_no_offered_answer_is_skipped) {
  const char* commands = R"([{"choice":"Which way?","store":"gold","answers":[
      {"text":"Left","if":[{"switch":"alarm"}],"do":[{"say":"l"}]},{"text":"Right","if":[{"switch":"saved"}],"do":[{"say":"r"}]}]},
      {"say":"end"}])";
  StoryWorld w = world();
  CHECK_STR(w.load(startEvents(commands)), "");
  CHECK(beginAsking(w));
  CHECK(!w.battle.choiceWaiting() && w.var("gold") == 0 && w.saidLine("end") && !w.saidLine("l"));
  StoryWorld v = world();
  CHECK(v.play(startEvents(commands)));  // the same when nobody is asked
  CHECK(v.var("gold") == 0 && v.saidLine("end"));
}

TEST(choice_an_earlier_answer_decides_which_answers_a_later_question_offers) {
  const char* commands = R"([{"choice":"One?","store":"gold","answers":[{"text":"x","do":[{"setSwitch":"saved","to":true}]},{"text":"y"}]},
      {"choice":"Two?","store":"kills","answers":[{"text":"only if x","if":[{"switch":"saved"}]},{"text":"always"},{"text":"never","if":[{"switch":"alarm"}]}]}])";
  StoryWorld w = world();
  CHECK_STR(w.load(startEvents(commands)), "");
  CHECK(beginAsking(w) && answer(w, 0));
  CHECK_EQ((int)w.last(EventType::Choice)->data.size(), 2);
  StoryWorld v = world();
  CHECK_STR(v.load(startEvents(commands)), "");
  CHECK(beginAsking(v) && answer(v, 1));
  CHECK(!v.battle.choiceWaiting() && v.count(EventType::Choice) == 1);  // "always" is the only one left: taken without asking
  CHECK_EQ(v.var("kills"), 2);
}

// ---- saving ----

TEST(choice_is_saved_and_a_saved_battle_goes_on_after_it) {
  StoryWorld w = world();
  CHECK_STR(w.load(startEvents(kGate)), "");
  CHECK(beginAsking(w) && answer(w, 1));
  CHECK(w.act(Action::endTurn()));  // some play after the choice
  Json saved;
  saveBattleState(w.battle, saved);
  Battle r;
  std::string err;
  CHECK(r.init(w.db, w.map, w.formulas, 1, err));
  CHECK(restoreBattleState(r, saved, err));
  CHECK_EQ(r.storyState.variables[(size_t)w.db.findVariable("gold")], 2);  // the choice came back
  CHECK(r.storyState.switches[(size_t)w.db.findSwitch("alarm")] != 0);
  CHECK(!r.choiceWaiting());
  CHECK_STR(savetest::stateText(r), savetest::stateText(w.battle));
  // The restored battle plays on; the question is not asked again.
  r.setAskChoices(true);
  std::vector<Event> ev;
  CHECK(r.apply(Action::endTurn(), ev));
  CHECK(!r.choiceWaiting() && std::count_if(ev.begin(), ev.end(), [](const Event& e) { return e.type == EventType::Choice; }) == 0);
}

TEST(choice_a_waiting_question_is_not_saved_and_the_state_is_the_question) {
  StoryWorld w = world();
  CHECK_STR(w.load(startEvents(kGate)), "");
  CHECK(beginAsking(w));
  CHECK(w.battle.choiceWaiting());
  // A copy of a waiting battle waits too, and can be answered independently.
  Battle copy = w.battle;
  std::vector<Event> ev;
  CHECK(copy.choiceWaiting() && copy.answerChoice(2, ev) && !copy.choiceWaiting() && copy.storyState.variables[(size_t)w.db.findVariable("gold")] == 3);
  CHECK(w.battle.choiceWaiting() && w.var("gold") == 0);
}

TEST(choice_the_game_without_asking_stays_byte_identical_to_one_without_a_choice) {
  // A map with no choice does not care whether asking is on.
  StoryWorld a = world(), b = world();
  const char* events = R"([{"id":"e","when":{"battle":"start"},"do":[{"say":"hi"},{"setSwitch":"saved","to":true}]}])";
  CHECK(a.play(events));
  CHECK_STR(b.load(events), "");
  CHECK(beginAsking(b));
  CHECK_STR(savetest::stateText(a.battle), savetest::stateText(b.battle));
  CHECK(savetest::sameEvents(a.events, b.events));
}
