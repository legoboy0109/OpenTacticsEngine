// Notices, choice cursors, faces, loops, typed names and scene backdrops in the core and the loader (docs/EVENTS.md "Speech bubbles,
// notices, names, faces and loops"): what the loader takes and refuses, what the battle and the scenes run, and what is saved.
#include "campaign_run.h"
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
