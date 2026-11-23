// Two small additions to the events (docs/EVENTS.md):
//   * `{ "battle": "beforePrep" }`: runs once before the battle-start events (and, in a player with a preparation screen, before the screen).
//   * `{ "actor": true }` as a unit filter inside the commands of an event that has an acting unit ("The actor as a target").
#include "battle_fixture.h"
#include "story_fixture.h"

using namespace ote;
using namespace story;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }
std::vector<std::string> field() { return {".........", ".........", "........."}; }

struct ItemWorld : StoryWorld {
  ItemWorld()
      : StoryWorld(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 0, 0, 2, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "lyra", "gorm"}) {
    fixture::addItems(*this);
  }
  int count(int unit, int item) const {
    int n = 0;
    for (const ItemStack& s : battle.units[(size_t)unit].items) n += s.item == item;
    return n;
  }
};

}  // namespace

// ---- beforePrep ----

TEST(before_prep_runs_before_the_battle_start_events_when_the_battle_just_starts) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play(R"([
    {"id":"b","when":{"battle":"start"},"do":[{"say":"battle"}]},
    {"id":"p","when":{"battle":"beforePrep"},"do":[{"say":"prep"},{"setVariable":"gold","to":5}]}])"));
  CHECK_EQ(w.said().size(), (size_t)2);
  CHECK(w.said()[0].find("prep") != std::string::npos);  // before, whatever the order in the file
  CHECK(w.said()[1].find("battle") != std::string::npos);
  CHECK_EQ(w.var("gold"), 5);
  CHECK(w.fired("p") && w.fired("b"));
}
TEST(before_prep_run_by_hand_is_not_run_again_by_start) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.load(R"([{"id":"p","when":{"battle":"beforePrep"},"do":[{"say":"prep"},{"addVariable":"gold","by":1}]}])").empty());
  Battle& b = w.battle;
  std::string err;
  CHECK(b.init(w.db, w.map, w.formulas, 1, err));
  CHECK(b.hasBeforePrep());
  std::vector<Event> first, second;
  b.runBeforePrep(first);
  CHECK_EQ(find(first, EventType::Dialogue), 0);
  b.start(second);
  CHECK_EQ(find(second, EventType::Dialogue), -1);  // it already ran
  CHECK_EQ(b.storyState.variables[(size_t)w.db.findVariable("gold")], 1);
}
TEST(before_prep_seen_runs_again_without_entries_for_its_state) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.load(R"([{"id":"p","when":{"battle":"beforePrep"},"do":[{"say":"prep"},{"setSwitch":"saved","to":true}]}])").empty());
  Battle& b = w.battle;
  std::string err;
  b.setBeforePrepSeen(true);  // a player that showed it before its preparation screen (set before init: init keeps it)
  CHECK(b.init(w.db, w.map, w.formulas, 1, err));
  std::vector<Event> ev;
  b.start(ev);
  CHECK_EQ(find(ev, EventType::Dialogue), -1);
  CHECK(b.storyState.switches[(size_t)w.db.findSwitch("saved")] != 0);
}
TEST(before_prep_changes_the_map_before_the_start_events_see_it) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play(R"([
    {"id":"b","when":{"battle":"start"},"if":[{"alive":{"unit":"guest"}}],"do":[{"setSwitch":"saved","to":true}]},
    {"id":"p","when":{"battle":"beforePrep"},"do":[{"addUnit":{"id":"guest","type":"soldier","side":"blue","x":4,"y":1}}]}])"));
  CHECK(w.sw("saved"));
  CHECK(w.unit("guest") >= 0);
}
TEST(before_prep_in_a_map_without_one_changes_nothing) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play(R"([{"id":"b","when":{"battle":"start"},"do":[{"say":"battle"}]}])"));
  CHECK(!w.battle.hasBeforePrep());
  CHECK_EQ(w.said().size(), (size_t)1);
}
TEST(before_prep_loader_refuses_what_makes_no_sense_before_deployment) {
  auto err = [&](const char* cmd) {
    StoryWorld v(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
    return v.load(std::string(R"([{"id":"p","when":{"battle":"beforePrep"},"do":[)") + cmd + "]}]");
  };
  CHECK(err(R"({"say":"x"},{"setSwitch":"saved","to":true},{"addUnit":{"type":"soldier","side":"red","x":5,"y":1}},{"focus":{"x":1,"y":1}})").empty());
  CHECK(has(err(R"({"win":true})"), "\"win\" makes no sense in a \"beforePrep\" event"));
  CHECK(has(err(R"({"lose":true})"), "\"lose\" makes no sense"));
  CHECK(has(err(R"({"endTurn":true})"), "\"endTurn\" makes no sense"));
  CHECK(has(err(R"({"choice":"Q?","answers":[{"text":"a","do":[{"say":"a"}]},{"text":"b","do":[{"say":"b"}]}]})"), "\"choice\" makes no sense"));
  CHECK(has(err(R"({"fight":{"attacker":"eirik","defender":"gorm"}})"), "\"fight\" makes no sense"));
  CHECK(has(err(R"({"if":[{"switch":"saved"}],"then":[{"win":true}]})"), "\"win\" makes no sense"));  // also nested
  ItemWorld iw;
  CHECK(has(iw.load(R"([{"id":"p","when":{"battle":"beforePrep"},"do":[{"giveItem":"potion","to":{"actor":true}}]}])"), "names the unit that set the event off"));  // nobody acts yet
  StoryWorld v(field(), {{"soldier", 0, 0, 1, false}}, {"eirik"});
  CHECK(has(v.load(R"([{"id":"p","when":{"battle":"soon"},"do":[{"say":"x"}]}])"), "\"beforePrep\""));  // the message lists it
}

// ---- the actor as a target ----

TEST(actor_target_gives_an_item_to_whoever_stopped_in_the_area_once) {
  ItemWorld w;
  CHECK(w.play(R"([{"id":"chest","when":{"reach":{"x":3,"y":0,"w":1,"h":3},"by":{"side":"blue"}},"do":[{"giveItem":"potion","to":{"actor":true}}]}])"));
  CHECK(w.act(Action::move(1, w.at(3, 2))));  // lyra stops there
  CHECK_EQ(w.count(1, fixture::items::Potion), 1);
  CHECK_EQ(w.count(0, fixture::items::Potion), 0);  // not eirik
  CHECK(w.act(Action::move(0, w.at(3, 1))));        // once: eirik finds nothing
  CHECK_EQ(w.count(0, fixture::items::Potion), 0);
}
TEST(actor_target_in_a_fight_is_the_attacker_and_take_item_works_the_same) {
  ItemWorld w;
  fixture::carry(w, 0, {fixture::items::Ring});
  w.map.units[2].x = 1;  // gorm next to eirik
  w.map.units[2].y = 1;
  CHECK(w.play(R"([{"id":"f","when":{"fight":{"side":"blue"}},"do":[{"giveItem":"potion","to":{"actor":true}},{"takeItem":"ring","from":{"actor":true}}]}])"));
  CHECK(w.act(Action::attack(0, 2, 0)));
  CHECK_EQ(w.count(0, fixture::items::Potion), 1);  // eirik attacked: he got it
  CHECK_EQ(w.count(0, fixture::items::Ring), 0);    // ... and lost his ring
  CHECK_EQ(w.count(2, fixture::items::Potion), 0);
}
TEST(actor_target_loader_refusals) {
  auto err = [&](const char* when, const char* cmd) {
    ItemWorld v;
    return v.load(std::string(R"([{"id":"e","when":)") + when + R"(,"do":[)" + cmd + "]}]");
  };
  const char* give = R"({"giveItem":"potion","to":{"actor":true}})";
  CHECK(err(R"({"reach":{"x":1,"y":1}})", give).empty());
  CHECK(err(R"({"defeated":{}})", give).empty());
  CHECK(err(R"({"fight":{}})", give).empty());
  CHECK(err(R"({"talk":"eirik","to":"lyra"})", give).empty());
  CHECK(err(R"({"battle":"start"})", give).find("\"actor\"") != std::string::npos);
  CHECK(has(err(R"({"battle":"start"})", give), "names the unit that set the event off"));
  CHECK(has(err(R"({"turn":1})", give), "names the unit that set the event off"));
  CHECK(has(err(R"({"battle":"won"})", give), "names the unit that set the event off"));
  CHECK(has(err(R"({"reach":{"x":1,"y":1},"by":{"actor":true}})", R"({"say":"x"})"), "not for triggers"));  // a trigger's own filter
  CHECK(has(err(R"({"reach":{"x":1,"y":1}})", R"({"giveItem":"potion","to":{"actor":false}})"), "must be true"));
  CHECK(has(err(R"({"reach":{"x":1,"y":1}})", R"({"giveItem":"potion","to":{"actor":true,"side":"blue"}})"), "more than one"));
  CHECK(err(R"({"reach":{"x":1,"y":1}})", R"({"takeItem":"potion","from":{"actor":true}},{"removeUnit":{"actor":true}},{"changeSide":{"actor":true},"side":"red"})").empty());
  ItemWorld v;  // an event's own `if` has the actor condition, not a filter
  CHECK(has(v.load(R"([{"id":"e","when":{"reach":{"x":1,"y":1}},"if":[{"alive":{"actor":true}}],"do":[{"say":"x"}]}])"), "names the unit that set the event off"));
  ItemWorld u;  // an objective is not an event
  CHECK(has(u.load("", R"({"win":[{"defeated":{"actor":true}}]})"), "names the unit that set the event off"));
}
