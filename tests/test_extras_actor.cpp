// The `actor` condition (docs/EVENTS.md "Branching on who acted"): the unit that set the running event off, as a condition.
// The actor in every kind of event that has one, false where none acts, `if` / `else` inside one event, a question's hidden answer, and the
// loader's refusals (a campaign scene has no actor).
#include "campaign_run.h"
#include "story_fixture.h"

using namespace ote;
using namespace story;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }
std::vector<std::string> field() { return {".........", ".........", "........."}; }

// eirik (0,0) and lyra (0,0) blue, gorm red: a fight and a reach are one move away.
struct ActorWorld : StoryWorld {
  ActorWorld()
      : StoryWorld(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 0, 0, 2, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "lyra", "gorm"}) {}
};

}  // namespace

// ---- actor ----

TEST(actor_reach_by_a_named_unit_branches_inside_one_event) {
  ActorWorld w;
  CHECK(w.play(R"([{"id":"gate","when":{"reach":{"x":3,"y":0,"w":1,"h":3}},"repeat":true,"do":[
      {"if":[{"actor":{"unit":"eirik"}}],"then":[{"addVariable":"gold","by":1}],"else":[{"addVariable":"kills","by":1}]}]}])"));
  CHECK(w.act(Action::move(0, w.at(3, 1))));  // eirik arrives
  CHECK_EQ(w.var("gold"), 1);
  CHECK_EQ(w.var("kills"), 0);
  CHECK(w.act(Action::move(1, w.at(3, 2))));  // lyra arrives
  CHECK_EQ(w.var("gold"), 1);
  CHECK_EQ(w.var("kills"), 1);
}
TEST(actor_filter_by_side_and_type_and_in_the_event_if) {
  ActorWorld w;
  CHECK(w.play(R"([
    {"id":"blue_only","when":{"reach":{"x":3,"y":0,"w":1,"h":3}},"if":[{"actor":{"side":"blue"}}],"do":[{"setSwitch":"saved","to":true}]},
    {"id":"red_only","when":{"reach":{"x":3,"y":0,"w":1,"h":3}},"if":[{"actor":{"side":"red"}}],"do":[{"setSwitch":"alarm","to":true}]},
    {"id":"by_type","when":{"reach":{"x":3,"y":0,"w":1,"h":3}},"if":[{"actor":{"type":"soldier"}}],"do":[{"setVariable":"gold","to":4}]}])"));
  CHECK(w.act(Action::move(0, w.at(3, 1))));
  CHECK(w.sw("saved") && !w.sw("alarm"));  // a blue unit moved: the event for red is not used up, it fails
  CHECK_EQ(w.var("gold"), 4);
}
TEST(actor_is_false_where_nobody_acts_and_the_story_started_it) {
  ActorWorld w;
  CHECK(w.play(R"([{"id":"t","when":{"turn":1},"do":[
      {"if":[{"actor":{}}],"then":[{"setSwitch":"saved","to":true}],"else":[{"setSwitch":"alarm","to":true}]}]},
    {"id":"b","when":{"battle":"start"},"if":[{"actor":{}}],"do":[{"setVariable":"gold","to":9}]}])"));
  CHECK(!w.sw("saved") && w.sw("alarm"));  // even "any unit" needs a unit that acted
  CHECK_EQ(w.var("gold"), 0);              // an event whose own `if` fails is not run
}
TEST(actor_in_a_fight_is_the_attacker_and_in_a_defeat_the_fallen) {
  ActorWorld w;
  w.map.units[2].x = 1;  // gorm next to eirik
  w.map.units[2].y = 1;
  CHECK(w.play(R"([
    {"id":"fight","when":{"fight":{"side":"blue"}},"do":[
       {"if":[{"actor":{"unit":"eirik"}}],"then":[{"setVariable":"gold","to":1}],"else":[{"setVariable":"gold","to":2}]}]},
    {"id":"fall","when":{"defeated":{"side":"red"}},"do":[
       {"if":[{"actor":{"unit":"gorm"}}],"then":[{"setVariable":"kills","to":7}]}]}])"));
  CHECK(w.act(Action::attack(0, 2, 0)));
  CHECK_EQ(w.var("gold"), 1);  // eirik attacked
}
TEST(actor_in_a_talk_is_the_talker) {
  ActorWorld w;
  w.map.units[1].x = 1;  // lyra next to eirik
  w.map.units[1].y = 1;
  w.map.units[0].x = 0;
  w.map.units[0].y = 1;
  CHECK(w.play(R"([{"id":"chat","when":{"talk":"eirik","to":"lyra"},"if":[{"actor":{"unit":"eirik"}}],"do":[{"setSwitch":"saved","to":true}]}])"));
  CHECK(w.act(Action::talk(0, 1)));
  CHECK(w.sw("saved"));
}
TEST(actor_hides_an_answer_of_a_question) {
  ActorWorld w;
  CHECK(w.play(R"([{"id":"q","when":{"reach":{"x":3,"y":0,"w":1,"h":3}},"repeat":true,"do":[
    {"choice":"Which way?","store":"kills","answers":[
      {"text":"Lyra's way","if":[{"actor":{"unit":"lyra"}}],"do":[{"say":"hers"}]},
      {"text":"Any way","do":[{"say":"any"}]}]}]}])"));
  CHECK(w.act(Action::move(0, w.at(3, 1))));  // eirik: the first answer is hidden, only "Any way" is left
  CHECK_EQ(w.var("kills"), 2);
  CHECK(w.act(Action::move(1, w.at(3, 2))));  // lyra: both offered, nobody is asked, the first one is taken
  CHECK_EQ(w.var("kills"), 1);
}
TEST(actor_loader_refusals_and_scenes) {
  ActorWorld w;
  auto err = [&](const char* cond) { ActorWorld v; return v.load(std::string(R"([{"id":"e","when":{"battle":"start"},"if":[)") + cond + R"(],"do":[{"say":"x"}]}])"); };
  CHECK(has(err(R"({"actor":{"unit":"nobody"}})"), "unknown unit \"nobody\""));
  CHECK(has(err(R"({"actor":{"side":"green"}})"), "unknown side"));
  CHECK(has(err(R"({"actor":"eirik","x":1})"), "unknown key"));
  CHECK(has(err(R"({"actor":5})"), "unit filter"));
  CHECK(err(R"({"actor":{}})").empty());
  CHECK(err(R"({"actor":"eirik"})").empty());
  camptest::Game g;
  const std::string bad = g.loadError("camp_actor_bad", R"({"chapters":[{"id":"c","name":"C","map":"ford","before":[{"if":[{"actor":{"unit":"eirik"}}],"then":[{"say":"x"}]}]}]})");
  CHECK(has(bad, "\"actor\" is for map events only"));
}

