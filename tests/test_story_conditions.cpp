// The story commands addCondition / removeCondition and the story condition hasCondition (docs/EVENTS.md "Status conditions",
// docs/SRPG.md section 9): giving and removing a condition on the units a filter picks, the turns override, immunity (a ward unit
// resists and the same ConditionResisted event comes), "all", the condition test in an event's `if`, the loader's refusals, and the
// campaign scene loader refusing all three. The conditions are built in code on top of the story fixture.
#include "campaign_run.h"
#include "story_fixture.h"

using namespace ote;
using namespace story;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }
std::vector<std::string> field() { return {".........", ".........", "........."}; }

int addHidden(Database& db, const char* id, SkillEffect fx) {
  SkillBlock b;
  b.when = SkillWhen::Always;
  b.effects.push_back(fx);
  SkillDef s;
  s.id = std::string("condition:") + id;
  s.name = id;
  s.hidden = true;
  s.blocks.push_back(b);
  db.skills.push_back(s);
  return (int)db.skills.size() - 1;
}

// Three conditions: "slow" (3 own turns, stops moving), "mark" (until cured), "ward" (3 turns, immune to "slow").
struct CondWorld : StoryWorld {
  CondWorld()
      : StoryWorld(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 0, 0, 2, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "lyra", "gorm"}) {
    SkillEffect move;
    move.kind = SkillEffect::Kind::CannotMove;
    ConditionDef slow;
    slow.id = "slow", slow.name = "Slow", slow.tag = "Sl", slow.turns = 3;
    slow.skill = addHidden(db, "slow", move);
    db.conditions.push_back(slow);
    ConditionDef mark;
    mark.id = "mark", mark.name = "Mark", mark.tag = "Mk", mark.until = ConditionUntil::Cured;
    mark.skill = addHidden(db, "mark", move);
    db.conditions.push_back(mark);
    SkillEffect immune;
    immune.kind = SkillEffect::Kind::Immune;
    immune.names = {"slow"};
    immune.conds = {0};
    ConditionDef ward;
    ward.id = "ward", ward.name = "Ward", ward.tag = "Wd", ward.turns = 3;
    ward.skill = addHidden(db, "ward", immune);
    db.conditions.push_back(ward);
  }
  int count(EventType t) const {
    int n = 0;
    for (const Event& e : events) n += e.type == t;
    return n;
  }
  const ConditionState* state(const char* unit, const char* cond) const {
    for (const ConditionState& cs : battle.units[(size_t)this->unit(unit)].conditions)
      if (cs.condition == db.findCondition(cond)) return &cs;
    return nullptr;
  }
};
std::string startEvent(const std::string& commands) { return "[{\"id\":\"e\",\"when\":{\"battle\":\"start\"},\"do\":" + commands + "}]"; }

}  // namespace

TEST(storycond_add_gives_the_condition_to_the_units_the_filter_picks) {
  CondWorld w;
  CHECK(w.play(startEvent(R"([{"addCondition":"slow","to":{"side":"blue"}}])")));
  CHECK(w.state("eirik", "slow") != nullptr && w.state("lyra", "slow") != nullptr);
  CHECK(w.state("gorm", "slow") == nullptr);  // the red unit is not matched
  CHECK_EQ(w.count(EventType::ConditionApplied), 2);
  CHECK_EQ(w.state("eirik", "slow")->left, 2);  // its own 3 turns, one already counted: the first turn of the battle has started
}
TEST(storycond_add_with_turns_overrides_and_a_second_add_starts_it_over) {
  CondWorld w;
  CHECK(w.play(R"([{"id":"a","when":{"battle":"start"},"do":[{"addCondition":"slow","to":"eirik","turns":5}]},
                   {"id":"b","when":{"turn":2},"do":[{"addCondition":"slow","to":"eirik","turns":2}]}])"));
  CHECK_EQ(w.state("eirik", "slow")->left, 4);  // 5 turns, the battle-start turn counted
  CHECK(w.act(Action::endTurn()) && w.act(Action::endTurn()));
  CHECK_EQ(w.state("eirik", "slow")->left, 2);  // starts over, never summed
  CHECK_EQ(w.battle.units[(size_t)w.unit("eirik")].conditions.size(), (size_t)1);
}
TEST(storycond_an_immune_unit_resists_with_the_usual_event) {
  CondWorld w;
  CHECK(w.play(R"([{"id":"a","when":{"battle":"start"},"do":[{"addCondition":"ward","to":"eirik"},{"addCondition":"slow","to":{"side":"blue"}}]}])"));
  CHECK(w.state("eirik", "slow") == nullptr);  // warded
  CHECK(w.state("lyra", "slow") != nullptr);
  CHECK_EQ(w.count(EventType::ConditionResisted), 1);
}
TEST(storycond_remove_one_all_and_nothing_to_remove) {
  CondWorld w;
  CHECK(w.play(R"([{"id":"a","when":{"battle":"start"},"do":[
      {"addCondition":"slow","to":{"side":"blue"}},{"addCondition":"mark","to":"eirik"},
      {"removeCondition":"slow","from":"eirik"},{"removeCondition":"mark","from":"lyra"}]}])"));
  CHECK(w.state("eirik", "slow") == nullptr && w.state("eirik", "mark") != nullptr);  // only slow left eirik
  CHECK(w.state("lyra", "slow") != nullptr);                                          // lyra never had mark: nothing happened
  CHECK_EQ(w.count(EventType::ConditionEnded), 1);
  CondWorld v;
  CHECK(v.play(R"([{"id":"a","when":{"battle":"start"},"do":[
      {"addCondition":"slow","to":"eirik"},{"addCondition":"mark","to":"eirik"},{"removeCondition":"all","from":{}}]}])"));
  CHECK(v.battle.units[(size_t)v.unit("eirik")].conditions.empty());
  CHECK_EQ(v.count(EventType::ConditionEnded), 2);
}
TEST(storycond_the_condition_test_decides_an_event) {
  CondWorld w;
  CHECK(w.play(R"([{"id":"a","when":{"battle":"start"},"do":[{"addCondition":"mark","to":"lyra"}]},
                   {"id":"b","when":{"turn":2},"if":[{"hasCondition":"mark","unit":"lyra"}],"do":[{"setSwitch":"alarm","to":true}]},
                   {"id":"c","when":{"turn":2},"if":[{"hasCondition":"mark","unit":"eirik"}],"do":[{"setSwitch":"saved","to":true}]},
                   {"id":"d","when":{"turn":2},"if":[{"hasCondition":"mark"}],"do":[{"setVariable":"gold","to":7}]}])"));
  CHECK(w.act(Action::endTurn()) && w.act(Action::endTurn()));
  CHECK(w.sw("alarm") && !w.sw("saved"));
  CHECK_EQ(w.var("gold"), 7);  // "unit" left out = any unit
}

TEST(storycond_the_loader_refuses_bad_forms_in_plain_words) {
  auto err = [](const std::string& cmd) { CondWorld w; return w.load(startEvent("[" + cmd + "]")); };
  CHECK(has(err(R"({"addCondition":"nothing","to":"eirik"})"), "\"addCondition\" names unknown condition \"nothing\" (data/conditions.json)"));
  CHECK(has(err(R"({"addCondition":"slow"})"), "needs \"to\""));
  CHECK(has(err(R"({"removeCondition":"slow"})"), "needs \"from\""));
  CHECK(has(err(R"({"addCondition":"slow","to":"eirik","turns":0})"), "\"turns\" must be a whole number from 1 to 99"));
  CHECK(has(err(R"({"addCondition":"mark","to":"eirik","turns":2})"), "does not count turns"));
  CHECK(has(err(R"({"removeCondition":"nothing","from":"eirik"})"), "unknown condition"));
  CHECK(has(err(R"({"removeCondition":"slow","from":"eirik","turns":2})"), "unknown key"));
  CHECK(has(err(R"({"addCondition":5,"to":"eirik"})"), "must be a condition id"));
  CHECK(err(R"({"removeCondition":"all","from":{}})").empty());
  // A game with no conditions says so.
  StoryWorld none(field(), {{"soldier", 0, 0, 1, false}}, {"eirik"});
  CHECK(has(none.load(startEvent(R"([{"addCondition":"slow","to":"eirik"}])")), "the game has no conditions (data/conditions.json)"));
  // The condition test.
  CondWorld w;
  CHECK(has(w.load(R"([{"id":"e","when":{"battle":"start"},"if":[{"hasCondition":"nothing"}],"do":[{"say":"x"}]}])"), "\"hasCondition\" names unknown condition"));
  CHECK(has(w.load(R"([{"id":"e","when":{"battle":"start"},"if":[{"hasCondition":"slow","x":1}],"do":[{"say":"x"}]}])"), "unknown key"));
  CHECK(w.load(R"([{"id":"e","when":{"battle":"start"},"if":[{"hasCondition":"slow","unit":{"side":"red"}}],"do":[{"say":"x"}]}])").empty());
}

// Conditions end with the map, so a campaign scene cannot give, remove or ask about one: the loader says so.
TEST(storycond_a_campaign_scene_refuses_all_three) {
  camptest::Game g;
  auto bad = [&](const char* cmd) {
    return g.loadError("camp_storycond_bad", std::string(R"({"chapters":[{"id":"c","name":"C","map":"ford","before":[)") + cmd + "]}]}");
  };
  CHECK(has(bad(R"({"addCondition":"poison","to":"eirik"})"), "\"addCondition\" is for map events only"));
  CHECK(has(bad(R"({"removeCondition":"all","from":"eirik"})"), "\"removeCondition\" is for map events only"));
  CHECK(has(bad(R"({"if":[{"hasCondition":"poison"}],"then":[{"say":"x"}]})"), "\"hasCondition\" is for map events only"));
}
