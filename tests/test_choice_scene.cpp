// Dialogue choices in campaign scenes (docs/EVENTS.md "Dialogue choices"): the loader's refusals, the first answer
// when nobody is asked, a question that stops the scene until it is answered (replay from the playthrough as it was),
// hidden answers, a branch to another chapter from an answer, and that the choice is saved with the playthrough.
#include "campaign_run.h"

using namespace camptest;

namespace {

bool has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

std::string refuse(const std::string& campaign) {
  Game g;
  return g.loadError("choice_scene", campaign);
}

// One chapter "a" on the ford map with the given "before" and "after" scene lists, then chapters b and c.
std::string camp(const std::string& before, const std::string& after = "[]") {
  return R"({ "chapters": [ { "id": "a", "map": "ford", "before": )" + before + R"(, "after": )" + after +
         R"( }, { "id": "b", "map": "mill", "before": [ {"say":"Chapter b."} ] },
             { "id": "c", "map": "keep", "before": [ {"say":"Chapter c."} ] } ] })";
}

const char* kAsk = R"([ {"say":"Before."},
  {"choice":"Spare him?","speakerName":"Eirik","store":"kills","answers":[
    {"text":"Spare","do":[{"say":"Spared."},{"setSwitch":"met_wanderer","to":true}]},
    {"text":"Strike","do":[{"say":"Struck."},{"addVariable":"chapters","by":7}]},
    {"text":"Walk away"}]},
  {"say":"After."} ])";

// The branching ending: an answer names the chapter that follows.
const char* kBranch = R"([ {"choice":"Which road?","answers":[
    {"text":"North","do":[{"goto":"b"}]},
    {"text":"South","do":[{"say":"South it is."},{"goto":"c"}]}]} ])";

struct Pick {
  Run r;
  Playthrough start;  // the playthrough before the scene
  std::vector<int> picks;
  bool begin(const std::string& campaignJson) { return r.begin("choice_scene", campaignJson); }
  // The scene as far as the picks go, from the playthrough as it was before it.
  bool play(SceneKind k, const std::vector<int>& answers) {
    picks = answers;
    r.pt = start;
    r.lines.clear();
    return runScene(r.g.db, r.pt, k, r.lines, picks);
  }
  bool waiting() const { return !r.lines.empty() && r.lines.back().kind == SceneLine::Kind::Choice && r.lines.back().waiting; }
  int var(const char* id) const { return r.pt.variables[(size_t)r.g.db.findVariable(id)]; }
  bool sw(const char* id) const { return r.pt.switches[(size_t)r.g.db.findSwitch(id)] != 0; }
};

}  // namespace

// ---- the loader ----

TEST(choice_scene_loader_reads_a_question) {
  Game g;
  CHECK(g.load("choice_scene", camp(kAsk)));
  const SceneCommand& c = g.db.campaign->chapters[0].before[1];
  CHECK(c.kind == SceneCommand::Kind::Choice);
  CHECK_EQ((int)c.answers.size(), 3);
  CHECK_EQ(c.index, g.db.findVariable("kills"));
  CHECK_STR(g.db.campaign->texts[(size_t)c.answers[2].text], "Walk away");
  CHECK_EQ((int)c.answers[0].commands.size(), 2);
}

TEST(choice_scene_loader_refuses_bad_questions) {
  std::string e = refuse(camp(R"([{"choice":"Q?","answers":[{"text":"Only"}]}])"));
  CHECK(has(e, "chapter \"a\": \"before\": command 1 (choice)") && has(e, "has 1 answer; a question needs two to four"));
  CHECK(has(refuse(camp(R"([{"choice":"Q?","answers":[{"text":"a"},{"text":"b"},{"text":"c"},{"text":"d"},{"text":"e"}]}])")),
            "has 5 answers"));
  CHECK(has(refuse(camp(R"([{"choice":"Q?","answers":[{"text":"a"},{"text":""}]}])")), "answer 2: \"text\" must be the words"));
  CHECK(has(refuse(camp(R"([{"choice":"Q?","answers":[{"text":"a"},{"text":"a"}]}])")), "says the same as an earlier answer"));
  CHECK(has(refuse(camp(R"([{"choice":"Q?","answers":[{"text":"a"},{"text":"b","do":[{"dance":1}]}]}])")),
            "answer 2: \"do\": command 1: is not a scene command"));
  CHECK(has(refuse(camp(R"([{"choice":"Q?","store":"nope","answers":[{"text":"a"},{"text":"b"}]}])")), "unknown variable \"nope\""));
  CHECK(has(refuse(camp(R"([{"choice":"Q?","answers":[{"text":"a","if":[{"switch":"nope"}]},{"text":"b"}]}])")), "unknown switch"));
  CHECK(has(refuse(camp(R"([{"choice":"A?","store":"kills","answers":[{"text":"a"},{"text":"b"}]},
                            {"choice":"B?","store":"kills","answers":[{"text":"a"},{"text":"b"}]}])")),
            "already writes"));
  // The same variable in two different scenes is fine.
  CHECK_STR(refuse(camp(R"([{"choice":"A?","store":"kills","answers":[{"text":"a"},{"text":"b"}]}])",
                        R"([{"choice":"B?","store":"kills","answers":[{"text":"a"},{"text":"b"}]}])")), "");
}

TEST(choice_scene_loader_checks_a_branch_to_a_chapter) {
  CHECK_STR(refuse(camp("[]", kBranch)), "");
  CHECK(has(refuse(camp("[]", R"([{"choice":"Q?","answers":[{"text":"a","do":[{"goto":"nowhere"}]},{"text":"b"}]}])")),
            "\"goto\" names unknown chapter \"nowhere\""));
  // A nested question and an `if` inside an answer are checked as well.
  CHECK(has(refuse(camp("[]", R"([{"choice":"Q?","answers":[{"text":"a","do":[{"choice":"R?","answers":[{"text":"x","do":[{"goto":"zz"}]},{"text":"y"}]}]},{"text":"b"}]}])")),
            "unknown chapter \"zz\""));
  // A before scene may branch too (to another chapter, never its own).
  CHECK(has(refuse(camp(R"([{"choice":"Q?","answers":[{"text":"a","do":[{"goto":"a"}]},{"text":"b"}]}])")), "names its own chapter"));
  CHECK_STR(refuse(camp(kBranch)), "");
}

// ---- nobody to ask ----

TEST(choice_scene_without_asking_takes_the_first_offered_answer) {
  Run r;
  CHECK(r.begin("choice_scene", camp(kAsk)));
  CHECK(r.scene(SceneKind::Before));
  CHECK_STR(r.said(), "|Before.;Eirik|Spare him?;|Spared.;|After.;");
  CHECK_EQ(r.pt.variables[(size_t)r.g.db.findVariable("kills")], 1);
  CHECK(r.pt.switches[(size_t)r.g.db.findSwitch("met_wanderer")] != 0);
  CHECK(r.pt.phase == Phase::Battle);
  for (const SceneLine& l : r.lines) CHECK(l.kind == SceneLine::Kind::Say);
}

// ---- a question that stops the scene ----

TEST(choice_scene_stops_at_the_question_and_goes_on_with_the_answer) {
  struct Path { int pick; int kills, chapters; bool met; const char* said; };
  const Path paths[] = {{0, 1, 0, true, "Spared."}, {1, 2, 7, false, "Struck."}, {2, 3, 0, false, "After."}};
  for (const Path& p : paths) {
    Pick k;
    CHECK(k.begin(camp(kAsk)));
    k.start = k.r.pt;
    CHECK(k.play(SceneKind::Before, {}));
    CHECK(k.waiting());
    CHECK_EQ((int)k.r.lines.size(), 2);  // "Before." and the question
    const SceneLine& q = k.r.lines.back();
    CHECK(q.speaker == "Eirik" && q.text == "Spare him?" && q.answers.size() == 3 && q.answers[1] == "Strike");
    CHECK(k.r.pt.phase == Phase::Before);       // the scene is not over
    CHECK_EQ(k.var("kills"), 0);                 // nothing of the answer yet
    CHECK(k.play(SceneKind::Before, {p.pick}));
    CHECK(!k.waiting() && k.r.pt.phase == Phase::Battle);
    CHECK_EQ(k.var("kills"), p.kills);
    CHECK_EQ(k.var("chapters"), p.chapters);
    CHECK_EQ(k.sw("met_wanderer") ? 1 : 0, p.met ? 1 : 0);
    // The same lines come first (the player skips what it has shown), then the answer's and the rest.
    CHECK(k.r.lines.size() >= 3 && k.r.lines[0].text == "Before." && k.r.lines[1].kind == SceneLine::Kind::Choice && !k.r.lines[1].waiting);
    CHECK(k.r.said().find(p.said) != std::string::npos && k.r.lines.back().text == "After.");
  }
}

TEST(choice_scene_answered_run_equals_writing_the_answer_out) {
  Pick a;
  CHECK(a.begin(camp(kAsk)));
  a.start = a.r.pt;
  CHECK(a.play(SceneKind::Before, {1}));
  Run b;
  CHECK(b.begin("choice_scene", camp(R"([ {"say":"Before."},{"say":"Spare him?","speakerName":"Eirik"},{"setVariable":"kills","to":2},
                                           {"say":"Struck."},{"addVariable":"chapters","by":7},{"say":"After."} ])")));
  CHECK(b.scene(SceneKind::Before));
  CHECK_STR(a.r.state(), b.state());
}

TEST(choice_scene_two_questions_and_a_nested_one) {
  const char* scene = R"([ {"choice":"One?","store":"kills","answers":[
      {"text":"x","do":[{"choice":"Inner?","store":"chapters","answers":[{"text":"p","do":[{"say":"p"}]},{"text":"q","do":[{"say":"q"}]}]}]},
      {"text":"y"}]},
    {"choice":"Two?","answers":[{"text":"m","do":[{"setSwitch":"bridge_cut","to":true}]},{"text":"n"}]} ])";
  Pick k;
  CHECK(k.begin(camp(scene)));
  k.start = k.r.pt;
  CHECK(k.play(SceneKind::Before, {}) && k.waiting() && k.r.lines.back().text == "One?");
  CHECK(k.play(SceneKind::Before, {0}) && k.waiting() && k.r.lines.back().text == "Inner?");
  CHECK(k.play(SceneKind::Before, {0, 1}) && k.waiting() && k.r.lines.back().text == "Two?");
  CHECK(k.play(SceneKind::Before, {0, 1, 0}) && !k.waiting());
  CHECK(k.var("kills") == 1 && k.var("chapters") == 2 && k.sw("bridge_cut") && k.r.pt.phase == Phase::Battle);
  CHECK(k.r.said().find("q") != std::string::npos && k.r.said().find("p;") == std::string::npos);
}

TEST(choice_scene_hides_answers_whose_condition_fails) {
  const char* scene = R"([ {"choice":"Where?","store":"kills","answers":[
      {"text":"Secret door","if":[{"switch":"bridge_cut"}]},{"text":"Gate"},{"text":"Wall","if":[{"variable":"chapters","op":">=","value":3}]},{"text":"Window"}]} ])";
  Pick k;
  CHECK(k.begin(camp(scene)));
  k.start = k.r.pt;
  CHECK(k.play(SceneKind::Before, {}) && k.waiting());
  CHECK(k.r.lines.back().answers == std::vector<std::string>({"Gate", "Window"}));
  CHECK(k.play(SceneKind::Before, {1}) && !k.waiting());
  CHECK_EQ(k.var("kills"), 4);  // "Window" is the fourth written answer
  // Every answer but one hidden: taken without asking; none: skipped.
  Pick one;
  CHECK(one.begin(camp(R"([ {"choice":"Q?","store":"kills","answers":[{"text":"a","if":[{"switch":"bridge_cut"}]},{"text":"b"}]},{"say":"end"} ])")));
  one.start = one.r.pt;
  CHECK(one.play(SceneKind::Before, {}) && !one.waiting() && one.var("kills") == 2 && one.r.said() == "|end;");
  Pick none;
  CHECK(none.begin(camp(R"([ {"choice":"Q?","store":"kills","answers":[{"text":"a","if":[{"switch":"bridge_cut"}]},{"text":"b","if":[{"switch":"met_wanderer"}]}]},{"say":"end"} ])")));
  none.start = none.r.pt;
  CHECK(none.play(SceneKind::Before, {}) && !none.waiting() && none.var("kills") == 0 && none.r.said() == "|end;");
}

// ---- branching ----

TEST(choice_scene_answer_picks_the_next_chapter) {
  struct Path { int pick; const char* next; };
  const Path paths[] = {{0, "b"}, {1, "c"}};
  for (const Path& p : paths) {
    Pick k;
    CHECK(k.begin(camp("[]", kBranch)));
    k.start = k.r.pt;
    CHECK(k.r.scene(SceneKind::Before));
    CHECK(k.r.startBattle());  // chapter a's battle is skipped over: decide it
    k.r.decide(true);
    CHECK(finishChapterBattle(k.r.g.db, k.r.pt, k.r.b) == ChapterResult::Won);
    k.start = k.r.pt;  // the playthrough before the after scene
    CHECK(k.play(SceneKind::After, {}));
    CHECK(k.waiting() && k.r.pt.phase == Phase::After && k.r.pt.chapter == 0);  // the chapter is not left yet
    CHECK(k.play(SceneKind::After, {p.pick}));
    CHECK(!k.waiting() && k.r.pt.phase == Phase::Before);
    CHECK_STR(currentChapter(k.r.g.db, k.r.pt)->id, p.next);
  }
}

TEST(choice_scene_a_variable_can_drive_a_later_branch) {
  // "An answer sets a variable, a condition branches": the way a choice of one chapter decides a later one.
  const std::string json = R"({ "chapters": [
    { "id": "a", "map": "ford", "before": [ {"choice":"Trust him?","store":"chapters","answers":[{"text":"Yes"},{"text":"No"}]} ],
      "after": [ {"if":[{"variable":"chapters","op":"=","value":2}],"then":[{"goto":"c"}]} ] },
    { "id": "b", "map": "mill" }, { "id": "c", "map": "keep" } ] })";
  for (int pick = 0; pick < 2; ++pick) {
    Pick k;
    CHECK(k.begin(json));
    k.start = k.r.pt;
    CHECK(k.play(SceneKind::Before, {pick}));
    CHECK(k.r.startBattle());  // (the scene already ran in the Run's copy: begin from the played playthrough)
    k.r.decide(true);
    CHECK(finishChapterBattle(k.r.g.db, k.r.pt, k.r.b) == ChapterResult::Won);
    std::vector<SceneLine> lines;
    CHECK(runScene(k.r.g.db, k.r.pt, SceneKind::After, lines));
    CHECK_STR(currentChapter(k.r.g.db, k.r.pt)->id, pick == 1 ? "c" : "b");
  }
}

// ---- saving ----

TEST(choice_scene_the_choice_survives_a_save) {
  Pick k;
  CHECK(k.begin(camp(kAsk)));
  k.start = k.r.pt;
  CHECK(k.play(SceneKind::Before, {1}));
  Json saved;
  savePlaythrough(k.r.g.db, k.r.pt, saved);
  Playthrough back;
  std::string err;
  CHECK(restorePlaythrough(k.r.g.db, saved, back, err));
  CHECK_EQ(back.variables[(size_t)k.r.g.db.findVariable("kills")], 2);
  CHECK(back.phase == Phase::Battle);
  // A battle of the chapter starts with the choice in its variables.
  Battle b;
  std::vector<Event> ev;
  CHECK(startChapterBattle(k.r.g.db, back, k.r.g.formulas, 1, b, ev, err));
  CHECK_EQ(b.storyState.variables[(size_t)k.r.g.db.findVariable("kills")], 2);
}

// ---- the data hash: wording is free, rules are not ----

namespace {
std::string hashOf(const std::string& campaign) {
  Game g;
  std::string hash, err;
  if (!g.load("choice_scene_hash", campaign) || !computeCampaignHash(g.db, hash, err)) return "ERROR " + err;
  return hash;
}
}  // namespace

TEST(choice_scene_hash_ignores_wording_but_not_what_the_answers_do) {
  const std::string base = camp(kAsk);
  std::string reworded = base;
  int n = 0;
  for (const char* from : {"Spare him?", "Spare", "Strike", "Walk away"}) {
    size_t at = reworded.find(std::string("\"") + from + "\"");
    CHECK(at != std::string::npos);
    if (at != std::string::npos) reworded.replace(at + 1, std::string(from).size(), "Other words " + std::to_string(++n));
  }
  CHECK_STR(hashOf(base), hashOf(reworded));
  std::string other = base;
  other.replace(other.find("\"by\":7"), 6, "\"by\":8");  // what an answer does
  CHECK(hashOf(base) != hashOf(other));
  std::string stored = base;
  stored.replace(stored.find("\"store\":\"kills\""), 15, "\"store\":\"chapters\"");
  CHECK(hashOf(base) != hashOf(stored));
}

// ---- a question in the very first scene, branching forward (a tutorial the player may skip) ----

namespace {
// "start" is the chapter whose before scene asks (speakerName only: no character exists yet); its battle is never played.
const char* kTutorial = R"({ "chapters": [
  { "id": "start", "map": "ford", "before": [
      {"choice":"Play the tutorial?","speakerName":"Guide","store":"chapters","answers":[
        {"text":"Yes","do":[{"setSwitch":"met_wanderer","to":true},{"goto":"tutorial"}]},
        {"text":"No, straight to chapter 1","do":[{"goto":"ch1"}]}]} ] },
  { "id": "tutorial", "map": "mill", "before": [ {"say":"Welcome to the tutorial.","speakerName":"Guide"} ] },
  { "id": "ch1", "map": "keep", "before": [
      {"if":[{"switch":"met_wanderer"}],"then":[{"say":"You know the basics.","speakerName":"Guide"}],"else":[{"say":"Good luck, no lessons.","speakerName":"Guide"}]} ] } ] })";
}  // namespace

TEST(choice_scene_first_scene_question_branches_forward_and_the_answer_stays_readable) {
  struct Path { int pick; const char* chapter; const char* line; };
  const Path paths[] = {{0, "tutorial", "Welcome to the tutorial."}, {1, "ch1", "Good luck, no lessons."}};
  for (const Path& p : paths) {
    Pick k;
    CHECK(k.begin(kTutorial));
    k.start = k.r.pt;
    CHECK(k.play(SceneKind::Before, {}) && k.waiting());
    CHECK_STR(k.r.lines.back().speaker, "Guide");     // a free speaker name: nobody is in the roster yet
    CHECK(k.r.lines.back().speakerId.empty());
    CHECK(k.play(SceneKind::Before, {p.pick}) && !k.waiting());
    CHECK_STR(currentChapter(k.r.g.db, k.r.pt)->id, p.chapter);
    CHECK(k.r.pt.phase == Phase::Before);              // the chapter branched to starts with its own before scene
    CHECK_EQ(k.var("chapters"), p.pick + 1);           // the answer is saved
    // The next scene reads it; the order goes on normally from the chapter branched to.
    k.start = k.r.pt;
    CHECK(k.play(SceneKind::Before, {}));
    CHECK(k.r.said().find(p.line) != std::string::npos);
    if (p.pick == 0) {  // the tutorial's following chapter is ch1, which reads the switch the answer set
      CHECK(k.r.pt.phase == Phase::Battle);
      CHECK(k.r.startBattle());
      k.r.decide(true);
      CHECK(finishChapterBattle(k.r.g.db, k.r.pt, k.r.b) == ChapterResult::Won);
      std::vector<SceneLine> lines;
      CHECK(runScene(k.r.g.db, k.r.pt, SceneKind::After, lines));
      CHECK_STR(currentChapter(k.r.g.db, k.r.pt)->id, "ch1");
      CHECK(k.r.scene(SceneKind::Before));
      CHECK(k.r.said().find("You know the basics.") != std::string::npos);
    }
  }
}
