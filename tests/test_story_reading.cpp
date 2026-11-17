// Story variables that read a unit's numbers (docs/EVENTS.md "Reading a unit's numbers"): map events (live unit,
// level after a level-up, two units summed against a bar, a missing / gone unit reads 0), campaign scenes (the roster
// as carried between chapters), the loader's refusals, and that the result is an ordinary saved variable.
#include "campaign_run.h"
#include "save_helpers.h"
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
std::string cmdError(const std::string& command) {
  StoryWorld w = world();
  return w.load("[{\"id\":\"e\",\"when\":{\"battle\":\"start\"},\"do\":[" + command + "]}]");
}

// The worked example: gold = eirik's level + lyra's level; kills = 1 when the bar (7) is reached, else 2.
const char* kBar = R"([{"id":"sum","when":{"battle":"start"},"do":[
  {"setVariable":"gold","level":"eirik"},{"addVariable":"gold","level":"lyra"},
  {"if":[{"variable":"gold","op":">=","value":7}],"then":[{"setVariable":"kills","to":1}],"else":[{"setVariable":"kills","to":2}]}]}])";

// init + set the two levels + start (events run at start).
bool startWithLevels(StoryWorld& w, int eirik, int lyra) {
  std::string err = w.load(kBar);
  if (!err.empty()) return false;
  if (!w.formulas.init(w.db, err) || !w.battle.init(w.db, w.map, w.formulas, 1, err)) return false;
  w.battle.units[(size_t)w.unit("eirik")].level = eirik;
  w.battle.units[(size_t)w.unit("lyra")].level = lyra;
  w.battle.start(w.events);
  return true;
}

}  // namespace

TEST(reading_two_levels_reach_the_bar) {
  StoryWorld w = world();
  CHECK(startWithLevels(w, 4, 3));
  CHECK_EQ(w.var("gold"), 7);
  CHECK_EQ(w.var("kills"), 1);
}
TEST(reading_two_levels_below_the_bar) {
  StoryWorld w = world();
  CHECK(startWithLevels(w, 3, 3));
  CHECK_EQ(w.var("gold"), 6);
  CHECK_EQ(w.var("kills"), 2);
}
TEST(reading_after_a_level_up_in_the_battle) {
  StoryWorld w = world();
  CHECK(w.play(R"([{"id":"r","when":{"turn":2},"do":[{"setVariable":"gold","level":"eirik"}]}])"));
  w.battle.units[(size_t)w.unit("eirik")].level = 2;  // what a level-up does
  CHECK(w.act(Action::endTurn()) && w.act(Action::endTurn()));
  CHECK_EQ(w.var("gold"), 2);
}
TEST(reading_hp_maxhp_and_a_stat) {
  StoryWorld w = world();
  CHECK(w.play(R"([{"id":"r","when":{"battle":"start"},"do":[
    {"setVariable":"gold","hp":"eirik"},{"setVariable":"kills","stat":"atk","of":"eirik"},
    {"addVariable":"kills","maxHp":"eirik"}]}])"));
  Unit& u = w.battle.units[(size_t)w.unit("eirik")];
  CHECK_EQ(w.var("gold"), u.hp);
  CHECK_EQ(w.var("kills"), w.battle.stat(u, w.db.findStat("atk")) + w.battle.maxHp(u));
}
TEST(reading_a_missing_removed_or_defeated_unit_is_zero) {
  StoryWorld w = world();
  CHECK(w.play(R"([{"id":"a","when":{"battle":"start"},"do":[
    {"removeUnit":"lyra"},{"setVariable":"gold","to":9},{"addVariable":"gold","level":"lyra"},
    {"setVariable":"kills","to":5},{"setVariable":"kills","level":"gorm"}]}])"));
  CHECK_EQ(w.var("gold"), 9);   // a removed unit adds 0
  CHECK_EQ(w.var("kills"), 1);  // gorm exists: level 1 (control)
  StoryWorld v = world();
  CHECK(v.play(R"([{"id":"a","when":{"battle":"start"},"do":[
    {"removeUnit":"gorm"},{"setVariable":"kills","to":5},{"setVariable":"kills","level":"gorm"}]}])"));
  CHECK_EQ(v.var("kills"), 0);  // gone reads 0
}
TEST(reading_a_unit_added_later_reads_zero_until_it_arrives) {
  StoryWorld w = world();
  CHECK(has(w.load(R"([{"id":"a","when":{"battle":"start"},"do":[{"setVariable":"kills","level":"late"}]}])"),
            "names unknown unit \"late\" (give the unit an \"id\" in the map's units)"));
  // A unit an earlier event adds is a known id: it reads 0 before it exists and its level afterwards.
  CHECK(w.play(R"([{"id":"a","when":{"battle":"start"},"do":[{"setVariable":"gold","level":"late"},
    {"addUnit":{"id":"late","type":"soldier","side":"blue","x":1,"y":0,"level":3}},{"setVariable":"kills","level":"late"}]}])"));
  CHECK_EQ(w.var("gold"), 0);
  CHECK_EQ(w.var("kills"), 3);
}
TEST(reading_add_form_accumulates) {
  StoryWorld w = world();
  CHECK(w.play(R"([{"id":"a","when":{"battle":"start"},"do":[
    {"setVariable":"gold","to":10},{"addVariable":"gold","level":"eirik"},{"addVariable":"gold","level":"eirik"}]}])"));
  CHECK_EQ(w.var("gold"), 12);
}
TEST(reading_result_is_saved_with_the_battle) {
  StoryWorld w = world();
  CHECK(w.play(R"([{"id":"a","when":{"battle":"start"},"do":[{"setVariable":"gold","level":"eirik"},{"addVariable":"gold","level":"eirik"}]}])"));
  Json saved;
  saveBattleState(w.battle, saved);
  Battle r;
  std::string err;
  CHECK(r.init(w.db, w.map, w.formulas, 1, err));
  CHECK(restoreBattleState(r, saved, err));
  CHECK_EQ(r.storyState.variables[(size_t)w.db.findVariable("gold")], 2);
}

TEST(reading_loader_refusals) {
  CHECK(has(cmdError(R"({"setVariable":"gold","to":3,"level":"eirik"})"), "give \"to\" (a number) or a reading from a unit, not both"));
  CHECK(has(cmdError(R"({"addVariable":"gold","by":3,"stat":"atk","of":"eirik"})"), "give \"by\" (a number)"));
  CHECK(has(cmdError(R"({"setVariable":"gold","level":"eirik","hp":"eirik"})"), "read one thing"));
  CHECK(has(cmdError(R"({"setVariable":"gold","level":""})"), "must be a unit id (text that is not empty)"));
  CHECK(has(cmdError(R"({"setVariable":"gold","level":"nobody"})"), "names unknown unit \"nobody\""));
  CHECK(has(cmdError(R"({"setVariable":"gold","stat":"luck","of":"eirik"})"), "unknown stat \"luck\""));
  CHECK(has(cmdError(R"({"setVariable":"gold","stat":"atk"})"), "needs \"of\""));
  CHECK(has(cmdError(R"({"setVariable":"gold","level":"eirik","of":"eirik"})"), "\"of\" goes with \"stat\" only"));
  CHECK(has(cmdError(R"({"setVariable":"gold","of":"eirik"})"), "\"of\" goes with \"stat\""));
  CHECK(has(cmdError(R"({"setVariable":"gold"})"), "missing required"));
  CHECK_STR(cmdError(R"({"setVariable":"gold","stat":"atk","of":"gorm"})"), "");
}

// ---- campaign scenes ----

namespace {
const char* kTwoChapters = R"({
  "name": "Reading", "rules": { "fallen": "gone", "onDefeat": "retry" },
  "chapters": [
    { "id": "ch1", "name": "One", "map": "ford",
      "after": [ {"setVariable":"kills","level":"eirik"}, {"addVariable":"kills","level":"lyra"} ] },
    { "id": "ch2", "name": "Two", "map": "mill",
      "before": [ {"setVariable":"chapters","level":"eirik"}, {"addVariable":"chapters","maxHp":"wanderer"},
                  {"addVariable":"chapters","stat":"atk","of":"nobody"}, {"addVariable":"chapters","level":"nobody"} ] }
  ] })";
}

TEST(reading_in_scenes_uses_the_roster_between_chapters) {
  camptest::Run r;
  CHECK(r.begin("camp_reading", kTwoChapters) && r.startBattle());
  r.b.units[(size_t)r.unit("eirik")].level = 4;
  r.b.units[(size_t)r.unit("lyra")].level = 3;
  CHECK(r.winAndFinish() == ChapterResult::Won);
  CHECK(r.scene(SceneKind::After));
  CHECK_EQ(r.pt.variables[(size_t)r.g.db.findVariable("kills")], 7);  // the after scene read the carried roster
  // Chapter two: eirik's carried level 4 + wanderer's max hp (4, a grunt) + two readings of a character not in the roster (0).
  CHECK(r.scene(SceneKind::Before));
  CHECK_EQ(r.pt.variables[(size_t)r.g.db.findVariable("chapters")], 4 + 4);
}
TEST(reading_in_scenes_a_gone_character_reads_zero) {
  camptest::Run r;
  CHECK(r.begin("camp_reading_gone", kTwoChapters) && r.startBattle());
  r.b.units[(size_t)r.unit("eirik")].level = 4;
  r.kill("eirik");  // "fallen": "gone"
  r.b.units[(size_t)r.unit("lyra")].level = 3;
  CHECK(r.winAndFinish() == ChapterResult::Won);
  CHECK(r.scene(SceneKind::After));
  CHECK_EQ(r.pt.variables[(size_t)r.g.db.findVariable("kills")], 3);
}
TEST(reading_in_scenes_loader_refusals) {
  camptest::Game g;
  auto bad = [&](const char* cmd) {
    return g.loadError("camp_reading_bad", std::string(R"({"chapters":[{"id":"c","name":"C","map":"ford","before":[)") + cmd + "]}]}");
  };
  CHECK(has(bad(R"({"setVariable":"kills","to":1,"level":"eirik"})"), "not both"));
  CHECK(has(bad(R"({"setVariable":"kills","hp":"eirik"})"), "\"hp\" cannot be read in a campaign scene"));
  CHECK(has(bad(R"({"setVariable":"kills","stat":"luck","of":"eirik"})"), "unknown stat"));
  CHECK(has(bad(R"({"setVariable":"kills","level":""})"), "not empty"));
  CHECK(has(bad(R"({"setVariable":"kills","level":"bad id"})"), "roster unit id"));
  CHECK_STR(bad(R"({"setVariable":"kills","maxHp":"eirik"})"), "");
}
