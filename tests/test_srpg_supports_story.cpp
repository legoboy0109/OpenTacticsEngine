// Supports in map events (docs/SRPG.md section 6 "Story and campaign"): the command addSupport, the condition
// support and the trigger support, and what the loader refuses.
#include "srpg_supports_fixture.h"

using namespace suptest;

namespace {

const Lv kPlain{10, {}, {}};
const Lv kTalk{10, {}, {"eirik: Well met."}};

std::string ev(const std::string& when, const std::string& doList, const std::string& cond = "") {
  return "{\"id\":\"ev\",\"when\":" + when + (cond.empty() ? "" : ",\"if\":" + cond) + ",\"do\":" + doList + "}";
}

}  // namespace

TEST(supports_story_add_support_gives_points_and_gains_a_level_without_conversation) {
  SupWorld w = trio();
  const int p = w.pair("eirik", "lyra", {kPlain, {50, {}, {"eirik: Later."}}});
  CHECK(w.play("[" + ev(R"({"battle":"start"})", R"([{"addSupport":["lyra","eirik"],"by":12}])") + "]"));  // either order
  CHECK_EQ(w.pts(p), 12);
  CHECK_EQ(w.level(p), 1);
  std::vector<Event> got = w.of(EventType::SupportPoints);
  CHECK_EQ(got.size(), (size_t)1);
  if (got.size() == 1) CHECK_EQ(got[0].d, 12);
  CHECK_EQ(w.of(EventType::SupportReached).size(), (size_t)1);
}

TEST(supports_story_negative_points_lower_the_level_and_stay_within_limits) {
  SupWorld w = trio();
  const int p = w.pair("eirik", "lyra", {kPlain}, 15);
  CHECK(w.play("[" + ev(R"({"battle":"start"})", R"([{"addSupport":["eirik","lyra"],"by":-8}])") + "]"));
  CHECK_EQ(w.pts(p), 7);
  CHECK_EQ(w.level(p), 0);  // the level never stands above what the points allow
}

TEST(supports_story_points_stay_between_zero_and_a_million) {
  SupWorld w = trio();
  const int p = w.pair("eirik", "lyra", {kPlain}, 3);
  CHECK(w.play(R"([{"id":"a","when":{"battle":"start"},"do":[{"addSupport":["eirik","lyra"],"by":-100}]},
                   {"id":"b","when":{"turn":1,"side":"red"},"do":[{"addSupport":["eirik","lyra"],"by":1000000},
                                                                 {"addSupport":["eirik","lyra"],"by":1000000}]}])"));
  CHECK_EQ(w.pts(p), 0);
  w.act(Action::endTurn());
  CHECK_EQ(w.pts(p), 1000000);
  CHECK_EQ(w.level(p), 1);
}

TEST(supports_story_condition_compares_the_pair_s_level) {
  SupWorld w = trio();
  w.pair("eirik", "lyra", {kPlain});
  const std::string start = R"({"battle":"start"})";
  CHECK(w.play("[" + ev(start, R"([{"setSwitch":"alarm","to":true}])", R"([{"support":["eirik","lyra"],"value":1}])") + "," +
               R"({"id":"give","when":{"battle":"start"},"do":[{"addSupport":["eirik","lyra"],"by":10}]},)" +
               R"({"id":"after","when":{"battle":"start"},"if":[{"support":["eirik","lyra"],"op":">=","value":1}],"do":[{"setSwitch":"saved","to":true}]},)" +
               R"({"id":"never","when":{"battle":"start"},"if":[{"support":["eirik","lyra"],"op":"=","value":2}],"do":[{"setVariable":"gold","to":9}]})" +
               "]"));
  CHECK(!w.sw("alarm"));  // first in the list: the pair had no level yet
  CHECK(w.sw("saved"));   // after the addSupport
  CHECK_EQ(w.var("gold"), 0);
}

TEST(supports_story_trigger_fires_when_the_pair_gains_that_level) {
  SupWorld w = trio();
  w.points(10);
  w.pair("eirik", "lyra", {kPlain, {20, {}, {}}, {30, {}, {}}});
  CHECK(w.play("[" + std::string(R"({"id":"any","when":{"support":["eirik","lyra"]},"do":[{"addVariable":"gold","by":1}],"repeat":true},)") +
               R"({"id":"second","when":{"support":["lyra","eirik"],"level":2},"do":[{"setSwitch":"alarm","to":true}]},)" +
               R"({"id":"third","when":{"support":["eirik","lyra"],"level":3},"do":[{"setSwitch":"saved","to":true}]})" + "]"));
  w.round();  // 10 points: level 1
  CHECK_EQ(w.var("gold"), 1);
  CHECK(!w.sw("alarm"));
  w.round();  // 20: level 2
  CHECK_EQ(w.var("gold"), 2);
  CHECK(w.sw("alarm") && !w.sw("saved"));
  w.round();
  CHECK(w.sw("saved"));
  CHECK_EQ(w.var("gold"), 3);
}

TEST(supports_story_trigger_fires_for_a_conversation_played_in_battle) {
  SupWorld w = trio();
  w.points(10);
  w.pair("eirik", "lyra", {kTalk});
  CHECK(w.play("[" + std::string(R"({"id":"seen","when":{"support":["eirik","lyra"],"level":1},"do":[{"say":"They talked."}]})") + "]"));
  w.round();
  CHECK(!w.fired("seen"));
  CHECK(w.supportAction("eirik", "lyra"));
  CHECK(w.fired("seen"));
  CHECK(w.saidLine("They talked."));
  const int reached = story::find(w.events, EventType::SupportReached);
  CHECK(reached >= 0 && story::find(w.events, EventType::Dialogue, (size_t)reached) > reached);  // the story line follows
}

TEST(supports_story_loader_refuses_what_is_not_a_declared_pair_or_level) {
  SupWorld w = trio();
  w.pair("eirik", "lyra", {kPlain, {20, {}, {}}});
  auto err = [&](const std::string& event) { return w.load("[" + event + "]"); };
  auto has = [](const std::string& hay, const char* needle) {
    if (hay.find(needle) == std::string::npos) std::printf("  got: %s\n  wanted: %s\n", hay.c_str(), needle);
    return hay.find(needle) != std::string::npos;
  };
  const std::string start = R"({"battle":"start"})";
  CHECK(has(err(ev(start, R"([{"addSupport":["eirik","gorm"],"by":1}])")), "\"eirik\" and \"gorm\" are not a support pair (data/supports.json)"));
  CHECK(has(err(ev(start, R"([{"addSupport":["eirik","nobody"],"by":1}])")), "unknown character \"nobody\""));
  CHECK(has(err(ev(start, R"([{"addSupport":["eirik"],"by":1}])")), "two character ids"));
  CHECK(has(err(ev(start, R"([{"addSupport":["eirik","lyra"]}])")), "missing required \"by\""));
  CHECK(has(err(ev(start, R"([{"addSupport":["eirik","lyra"],"by":1,"x":2}])")), "unknown key \"x\""));
  CHECK(has(err(ev(start, R"([{"addSupport":["eirik","lyra"],"by":2000000}])")), "\"by\" must be from -1000000 to 1000000"));
  CHECK(has(err(ev(start, "[]", R"([{"support":["eirik","gorm"],"value":1}])")), "are not a support pair"));
  CHECK(has(err(ev(start, "[]", R"([{"support":["eirik","lyra"],"op":"~","value":1}])")), "\"op\" must be one of"));
  CHECK(has(err(ev(start, "[]", R"([{"support":["eirik","lyra"]}])")), "missing required \"value\""));
  CHECK(has(err(ev(R"({"support":["eirik","lyra"],"level":3})", "[]")), "\"level\" must be from 1 to 2"));
  CHECK(has(err(ev(R"({"support":["eirik","gorm"]})", "[]")), "are not a support pair"));
  CHECK_STR(err(ev(R"({"support":["eirik","lyra"],"level":2})", "[]")), "");
}

TEST(supports_story_loader_in_a_game_without_pairs) {
  SupWorld w = trio();
  const std::string r = w.load("[" + ev(R"({"battle":"start"})", R"([{"addSupport":["eirik","lyra"],"by":1}])") + "]");
  CHECK(r.find("are not a support pair") != std::string::npos);
}
