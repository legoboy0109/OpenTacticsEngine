// Notices, choice cursors, faces, loops, typed names and scene backdrops in the core and the loader (docs/EVENTS.md "Speech bubbles,
// notices, names, faces and loops"): what the loader takes and refuses, what the battle and the scenes run, and what is saved.
#include <algorithm>

#include "campaign_run.h"
#include "core/campaign/campaign_save.h"
#include "core/data/name_tokens.h"
#include "core/data/scene_fx.h"
#include "story_fixture.h"

using namespace ote;
using namespace story;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

StoryWorld world() {
  return StoryWorld({".........", ".........", "........."},
                    {{"soldier", 0, 0, 1, false}, {"soldier", 0, 0, 2, false}, {"soldier", 1, 8, 1, false}},
                    {"eirik", "lyra", "gorm"});
}
std::string mapError(const std::string& commands) {
  StoryWorld w = world();
  return w.load("[{\"id\":\"e\",\"when\":{\"battle\":\"start\"},\"do\":[" + commands + "]}]");
}
std::string sceneError(const std::string& commands, const char* folder = "camp_dialogue_bad") {
  camptest::Game g;
  return g.loadError(folder, std::string(R"({"chapters":[{"id":"c","name":"C","map":"ford","before":[)") + commands + "]}]}");
}

// A battle in which questions wait for a person.
bool beginAsking(StoryWorld& w) {
  std::string err;
  if (!w.formulas.init(w.db, err) || !w.battle.init(w.db, w.map, w.formulas, 1, err)) return false;
  w.battle.setAskChoices(true);
  w.battle.start(w.events);
  return true;
}

// The spec's own example (docs/EVENTS.md "Notices", "Loops").
const char* kNoticeChoice = R"({"choice":"Repeat lesson?","notice":true,"cursor":2,
  "answers":[{"text":"Yes"},{"text":"No","do":[{"break":true}]}]})";

}  // namespace

// A "notice": true on a choice is the choice's modifier, not a second command (map events and campaign scenes alike).
TEST(dialogue_notice_true_on_a_choice_is_not_a_second_command) {
  CHECK_STR(mapError(std::string("{\"loop\":[") + kNoticeChoice + "]}"), "");
  CHECK_STR(mapError(R"({"choice":"Q?","notice":true,"answers":[{"text":"a"},{"text":"b"}]})"), "");
  CHECK_STR(sceneError(std::string("{\"loop\":[") + kNoticeChoice + "]}", "camp_dialogue_nc1"), "");
  CHECK_STR(sceneError(R"({"choice":"Q?","notice":false,"answers":[{"text":"a"},{"text":"b"}]})", "camp_dialogue_nc2"), "");
  // The notice command itself still is one, and still cannot share an entry with another command.
  CHECK_STR(mapError(R"({"notice":"Your clan has grown stronger."})"), "");
  CHECK(has(mapError(R"({"notice":"Hi","say":"Hi"})"), "has two commands"));
  CHECK(has(sceneError(R"({"notice":"Hi","say":"Hi"})", "camp_dialogue_nc3"), "has two commands"));
  // A true / false notice without a choice is no command at all.
  CHECK(!mapError(R"({"notice":true})").empty());
}

// ---- the loader's refusals

TEST(dialogue_loader_refusals_in_map_events) {
  CHECK(has(mapError(R"({"notice":""})"), "must be the text to show"));
  CHECK(has(mapError(R"({"notice":"Hi","speaker":"eirik"})"), "unknown key"));
  CHECK(has(mapError(R"({"choice":"Q?","notice":"yes","answers":[{"text":"a"},{"text":"b"}]})"), "notice"));
  CHECK(has(mapError(R"({"choice":"Q?","notice":true,"speaker":"eirik","answers":[{"text":"a"},{"text":"b"}]})"), "makes no sense"));
  CHECK(has(mapError(R"({"choice":"Q?","cursor":3,"answers":[{"text":"a"},{"text":"b"}]})"), "cursor"));
  CHECK(has(mapError(R"({"choice":"Q?","cursor":0,"answers":[{"text":"a"},{"text":"b"}]})"), "cursor"));
  CHECK(has(mapError(R"({"loop":[]})"), "loop"));
  CHECK(has(mapError(R"({"loop":[{"say":"a"}],"max":101})"), "max"));
  CHECK(has(mapError(R"({"loop":[{"say":"a"}],"times":2})"), "unknown key"));
  CHECK(has(mapError(R"({"break":true})"), "only works inside"));
  CHECK(has(mapError(R"({"loop":[{"break":false}]})"), "write { \"break\": true }"));
  CHECK(has(mapError(R"({"askName":"eirik"})"), "askName"));
  CHECK(has(mapError(R"({"say":"Hi {name:}","speaker":"eirik"})"), "{name:"));
  CHECK(has(mapError(R"({"say":"Hi","speaker":"eirik","face":16})"), "face"));
  // A break inside an if inside a loop is fine; so is a loop inside a loop.
  CHECK_STR(mapError(R"({"loop":[{"if":[{"switch":"saved"}],"then":[{"break":true}]},{"loop":[{"break":true}],"max":2}]})"), "");
}

TEST(dialogue_loader_refusals_in_campaign_scenes) {
  CHECK(has(sceneError(R"({"askName":"bad id"})", "camp_dialogue_r1"), "the id of the character"));
  CHECK(has(sceneError(R"({"askName":"eirik","max":25})", "camp_dialogue_r2"), "max"));
  CHECK(has(sceneError(R"({"askName":"eirik","default":"Eirikson","max":4})", "camp_dialogue_r3"), "more than \"max\""));
  CHECK(has(sceneError(R"({"askName":"eirik","default":"A{b}"})", "camp_dialogue_r4"), "\"default\""));
  CHECK(has(sceneError(R"({"askName":"eirik","prompt":""})", "camp_dialogue_r5"), "prompt"));
  CHECK(has(sceneError(R"({"break":true})", "camp_dialogue_r6"), "only works inside"));
  CHECK_STR(sceneError(R"({"askName":"eirik","default":"Eirik","max":8,"prompt":"Name {name:lyra}'s friend"})", "camp_dialogue_r7"), "");
  camptest::Game g;
  CHECK(has(g.loadError("camp_dialogue_r8", R"({"backdrop":"nowhere","chapters":[{"id":"c","name":"C","map":"ford"}]})"), "nowhere"));
}

// ---- the battle

TEST(dialogue_loop_breaks_on_the_cursor_answer_without_asking) {
  StoryWorld w = world();
  CHECK(w.play(std::string(R"([{"id":"e","when":{"battle":"start"},"do":[{"loop":[{"say":"Lesson","speaker":"eirik"},)") + kNoticeChoice +
               R"(]},{"say":"After","speaker":"eirik"}]}])"));
  int lessons = 0, questions = 0;
  for (const Event& e : w.events) {  // with nobody to ask, the question is only said (a Dialogue entry, in the notice window)
    if (e.type != EventType::Dialogue) continue;
    const std::string text = w.battle.storyText(e.b);
    if (text == "Lesson") ++lessons;
    if (text == "Repeat lesson?") {
      ++questions;
      CHECK((e.flags & kDialogueNotice) != 0);
    }
  }
  CHECK_EQ(lessons, 1);  // "No" (the cursor) breaks the loop after one round
  CHECK_EQ(questions, 1);
  CHECK(w.saidLine("After"));
}

TEST(dialogue_choice_entry_carries_notice_and_cursor_when_a_person_answers) {
  StoryWorld w = world();
  CHECK_STR(w.load(std::string(R"([{"id":"e","when":{"battle":"start"},"do":[{"loop":[)") + kNoticeChoice + "]}]}]"), "");
  CHECK(beginAsking(w));
  const int c = find(w.events, EventType::Choice);
  CHECK(c >= 0);
  if (c >= 0) {
    CHECK((w.events[(size_t)c].flags & kDialogueNotice) != 0);
    CHECK_EQ((w.events[(size_t)c].flags & kChoiceCursorMask) >> kChoiceCursorShift, 1);  // the second offered answer
  }
}

TEST(dialogue_loop_without_a_break_stops_at_its_cap) {
  StoryWorld w = world();
  CHECK(w.play(R"([{"id":"e","when":{"battle":"start"},"do":[{"loop":[{"addVariable":"gold","by":1}],"max":4},{"say":"Out","speaker":"eirik"}]}])"));
  CHECK_EQ(w.var("gold"), 4);
  CHECK(w.saidLine("Out"));
  StoryWorld d = world();
  CHECK(d.play(R"([{"id":"e","when":{"battle":"start"},"do":[{"loop":[{"addVariable":"gold","by":1}]}]}])"));
  CHECK_EQ(d.var("gold"), kDefaultLoopMax);
}

TEST(dialogue_break_ends_only_the_inner_loop) {
  StoryWorld w = world();
  CHECK(w.play(R"([{"id":"e","when":{"battle":"start"},"do":[
    {"loop":[{"addVariable":"gold","by":1},{"loop":[{"addVariable":"kills","by":1},{"break":true},{"addVariable":"kills","by":100}]}],"max":3}]}])"));
  CHECK_EQ(w.var("gold"), 3);
  CHECK_EQ(w.var("kills"), 3);
}

TEST(dialogue_loop_asks_again_each_round_when_a_person_answers) {
  StoryWorld w = world();
  CHECK_STR(w.load(std::string(R"([{"id":"e","when":{"battle":"start"},"do":[{"loop":[{"addVariable":"gold","by":1},)") + kNoticeChoice + "]}]}]"),
            "");
  CHECK(beginAsking(w));
  CHECK(w.battle.choiceWaiting());
  CHECK(w.battle.answerChoice(0, w.events));  // "Yes": again
  CHECK(w.battle.choiceWaiting());
  CHECK(w.battle.answerChoice(1, w.events));  // "No": out
  CHECK(!w.battle.choiceWaiting());
  CHECK_EQ(w.var("gold"), 2);
}

TEST(dialogue_say_carries_the_notice_flag) {
  // ("face" needs a portrait with expressions, which this world has none of: it is refused, see the loader test; the face flag is
  // checked by the player test's screenshot.)
  StoryWorld w = world();
  CHECK(has(w.load(R"([{"id":"e","when":{"battle":"start"},"do":[{"say":"Hm","speaker":"eirik","face":3}]}])"), "no portrait"));
  CHECK(w.play(R"([{"id":"e","when":{"battle":"start"},"do":[{"notice":"Hello"},{"say":"Plain","speaker":"eirik"},{"say":"Plain","speaker":"eirik"}]}])"));
  std::vector<int> flags;
  for (const Event& e : w.events)
    if (e.type == EventType::Dialogue) flags.push_back(e.flags);
  CHECK_EQ((int)flags.size(), 3);
  if (flags.size() == 3) {
    CHECK((flags[0] & kDialogueNotice) != 0);
    CHECK_EQ(flags[1] & (kDialogueNotice | kDialogueFaceMask), 0);
    CHECK_EQ(flags[2] & (kDialogueNotice | kDialogueFaceMask), 0);  // a plain line is unchanged
  }
}

TEST(dialogue_hero_names_rename_units_and_answer_tokens) {
  StoryWorld w = world();
  CHECK(w.play(R"([{"id":"e","when":{"battle":"start"},"do":[{"say":"I am {name:eirik}.","speaker":"eirik"}]}])"));
  CHECK_STR(w.battle.heroName("eirik"), w.battle.units[(size_t)w.unit("eirik")].name);
  w.battle.setHeroNames(std::make_shared<const Battle::HeroNames>(Battle::HeroNames{{"eirik", "Kai"}}));
  CHECK_STR(w.battle.units[(size_t)w.unit("eirik")].name, "Kai");
  CHECK_STR(w.battle.heroName("eirik"), "Kai");
  CHECK_STR(w.battle.heroName("nobody"), "nobody");
  CHECK_STR(expandNameTokens("Hi {name:eirik}, {name:lyra}!", [&](const std::string& id) { return w.battle.heroName(id); }),
            std::string("Hi Kai, ") + w.battle.heroName("lyra") + "!");
}

TEST(dialogue_name_tokens_and_entered_names) {
  std::string why;
  CHECK(nameTokensOk("Hi {name:eirik}", why));
  CHECK(!nameTokensOk("Hi {name:}", why));
  CHECK(!nameTokensOk("Hi {name:eirik", why));
  CHECK(validEnteredName("Kai"));
  CHECK(validEnteredName("\xc3\x89ric"));  // UTF-8 letters count as one each
  CHECK(!validEnteredName(""));
  CHECK(!validEnteredName("A{b}"));
  CHECK(!validEnteredName(std::string(25, 'a')));
}

// ---- the campaign: names in the playthrough and its save

TEST(dialogue_typed_names_rename_rosters_save_and_restore) {
  camptest::Run r;
  CHECK(r.begin("camp_dialogue_names") && r.startBattle());
  CHECK(r.winAndFinish() == ChapterResult::Won);
  CHECK(r.pt.findUnit("eirik") != nullptr);
  const std::string before = r.state();
  CHECK(before.find("\"names\"") == std::string::npos);  // nobody named: the save is as before
  CHECK(setTypedName(r.pt, "eirik", "Kai"));
  CHECK(!setTypedName(r.pt, "eirik", ""));
  CHECK(!setTypedName(r.pt, "bad id", "Kai"));
  CHECK_STR(r.pt.findUnit("eirik")->name, "Kai");
  CHECK_STR(characterName(r.g.db, r.pt, "eirik"), "Kai");
  Json j;
  savePlaythrough(r.g.db, r.pt, j);
  Playthrough back;
  std::string err;
  CHECK(restorePlaythrough(r.g.db, j, back, err));
  CHECK_STR(err, "");
  CHECK(typedName(back, "eirik") != nullptr && *typedName(back, "eirik") == "Kai");
  // A broken "names" is refused with its place.
  Json bad = j;  // (a copy)
  Json names = Json::makeObject();
  names.set("eirik", Json::makeString("A{b}"));
  bad.set("names", names);
  Playthrough no;
  CHECK(!restorePlaythrough(r.g.db, bad, no, err));
  CHECK(has(err, "names"));
}

TEST(dialogue_ask_name_in_a_scene_keeps_the_default_and_the_next_battle_uses_it) {
  const char* camp = R"({ "name": "Names", "rules": { "fallen": "gone", "onDefeat": "retry" },
    "chapters": [ { "id": "ch1", "name": "One", "map": "ford",
      "before": [ {"askName":"eirik","default":"Ash","max":6,"prompt":"Name?"}, {"say":"Go, {name:eirik}.","speaker":"eirik"} ] } ] })";
  camptest::Run r;
  CHECK(r.begin("camp_dialogue_ask", camp) && r.scene(SceneKind::Before));
  CHECK_EQ((int)r.lines.size(), 2);
  if (r.lines.size() == 2) {
    CHECK(r.lines[0].kind == SceneLine::Kind::AskName);
    CHECK_STR(r.lines[0].speakerId, "eirik");
    CHECK_STR(r.lines[0].speaker, "Ash");
    CHECK_EQ(r.lines[0].maxLetters, 6);
  }
  CHECK(typedName(r.pt, "eirik") != nullptr && *typedName(r.pt, "eirik") == "Ash");
  CHECK(r.startBattle());
  CHECK_STR(r.b.units[(size_t)r.unit("eirik")].name, "Ash");
}

TEST(dialogue_scene_loop_and_cursor_without_asking) {
  const char* camp = R"({ "name": "Loops", "rules": { "fallen": "gone", "onDefeat": "retry" },
    "chapters": [ { "id": "ch1", "name": "One", "map": "ford",
      "before": [ {"loop":[{"say":"Lesson"},{"choice":"Again?","notice":true,"cursor":2,
                   "answers":[{"text":"Yes"},{"text":"No","do":[{"break":true}]}]}],"max":5}, {"say":"Done"} ] } ] })";
  camptest::Run r;
  CHECK(r.begin("camp_dialogue_loop", camp) && r.scene(SceneKind::Before));
  int lessons = 0, notices = 0;
  for (const SceneLine& l : r.lines) {
    if (l.kind == SceneLine::Kind::Say && l.text == "Lesson") ++lessons;
    if (l.text == "Again?" && l.notice && l.cursor == 1) ++notices;  // (nobody to ask: the question is only said)
  }
  CHECK_EQ(lessons, 1);
  CHECK_EQ(notices, 1);
  CHECK(!r.lines.empty() && r.lines.back().text == "Done");
}

TEST(dialogue_game_json_dialogue_is_presentation_only) {
  // The option does not reach the battle's rules: the same battle plays the same with and without it.
  StoryWorld a = world(), b = world();
  b.db.dialogue.bubbles = true;
  b.db.dialogue.portrait = 1;
  const char* ev = R"([{"id":"e","when":{"battle":"start"},"do":[{"say":"Hi","speaker":"eirik"}]}])";
  CHECK(a.play(ev) && b.play(ev));
  CHECK_EQ((int)a.events.size(), (int)b.events.size());
  CHECK(a.said() == b.said());
}
