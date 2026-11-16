// Step R1 of docs/SRPG.md: the story's view of carried units. A carried unit still exists for the conditions
// "alive" / "dead" / "carries" and for the commands that take a filter; `giveItem` and "reach" never see it.
#include "rescue_fixture.h"

using namespace ote;
using namespace rescuetest;

namespace {

// Eirik (soldier, 1,1) and Lyra (archer, 2,1) are blue, Gorm is red; "c" and "d" are blue soldiers.
RescueWorld cast() {
  return RescueWorld(RescueMode::Carry, {{"soldier", 0, 1, 1, false}, {"archer", 0, 2, 1, false}, {"soldier", 0, 1, 2, false},
                                         {"soldier", 0, 0, 2, false}, {"soldier", 1, 4, 1, false}},
                     open(), {"eirik", "lyra", "c", "d", "gorm"});
}

// An event that runs when blue's second turn begins (after a rescue in the first).
std::string atTurn2(const std::string& commands) {
  return "[{\"id\":\"e\",\"when\":{\"turn\":2,\"side\":\"blue\"},\"do\":" + commands + "}]";
}

}  // namespace

TEST(rescue_story_alive_dead_and_carries_see_a_carried_unit) {
  RescueWorld w = cast();
  fixture::addItems(w);
  fixture::carry(w, 1, {fixture::items::Potion});
  CHECK(w.play(atTurn2(R"([
    {"if":[{"alive":"lyra"}],"then":[{"setSwitch":"saved","to":true}]},
    {"if":[{"dead":"lyra"}],"then":[{"setSwitch":"alarm","to":true}]},
    {"if":[{"carries":"potion","unit":{"unit":"lyra"}}],"then":[{"setVariable":"gold","to":7}]}])")));
  CHECK(w.act(Action::rescue(0, 1)));
  w.pass();
  CHECK(w.fired("e"));
  CHECK(w.sw("saved") && !w.sw("alarm"));  // not dead: carried is not defeated
  CHECK_EQ(w.var("gold"), 7);                // it keeps its items
}

TEST(rescue_story_remove_unit_takes_a_carried_unit_out_of_its_carrier) {
  RescueWorld w = cast();
  CHECK(w.play(atTurn2(R"([{"removeUnit":"lyra"}])")));
  CHECK(w.act(Action::rescue(0, 1)));
  w.pass();
  CHECK(!w.battle.exists(w.un(1)) && w.un(1).hp == 0 && w.un(1).carriedBy == -1);
  CHECK_EQ(w.un(0).carrying, -1);
  const Event* e = w.last(EventType::UnitRemoved);
  CHECK(e && e->a == 1 && e->b == w.at(1, 1));  // it leaves from where its carrier stands
  CHECK_EQ(w.count(EventType::UnitDied), 0);
  CHECK(w.stands(0, 1, 1) && !w.battle.sides[0].defeated);
}

TEST(rescue_story_remove_unit_on_a_carrier_puts_its_unit_down) {
  RescueWorld w = cast();
  CHECK(w.play(atTurn2(R"([{"removeUnit":"eirik"}])")));
  CHECK(w.act(Action::rescue(0, 1)));
  w.pass();
  CHECK(!w.battle.exists(w.un(0)));
  CHECK(w.stands(1, 1, 1) && w.un(1).done);  // on the carrier's square
  const Event* e = w.last(EventType::UnitDropped);
  CHECK(e && e->a == 0 && e->b == 1 && e->c == w.at(1, 1) && e->flags == kForced);
  CHECK_EQ(w.count(EventType::UnitDied), 0);  // removed, not defeated
}

TEST(rescue_story_move_unit_on_a_carried_unit_drops_it_on_the_square) {
  RescueWorld w = cast();
  CHECK(w.play(R"([{"id":"e","when":{"turn":2,"side":"blue"},"do":[{"moveUnit":"lyra","x":4,"y":0}]},
                   {"id":"r","when":{"reach":{"x":4,"y":0},"by":"lyra"},"do":[{"setSwitch":"alarm","to":true}]}])"));
  CHECK(w.act(Action::rescue(0, 1)));
  w.pass();
  CHECK(w.stands(1, 4, 0));
  CHECK_EQ(w.un(0).carrying, -1);
  const Event* e = w.last(EventType::UnitDropped);
  CHECK(e && e->a == 0 && e->b == 1 && e->c == w.at(4, 0) && e->flags == kForced);
  CHECK(w.sw("alarm"));  // it arrived there: "reach" events run
  CHECK(w.un(1).done);
}

TEST(rescue_story_change_side_puts_the_carried_unit_down_first) {
  RescueWorld w = cast();
  CHECK(w.play(atTurn2(R"([{"changeSide":"eirik","side":"red"}])")));
  CHECK(w.act(Action::rescue(0, 1)));
  w.pass();
  CHECK_EQ(w.un(0).side, 1);
  CHECK_EQ(w.un(1).side, 0);  // Lyra stays blue and is on the board again
  CHECK(w.un(1).alive && w.un(0).carrying == -1 && w.un(1).carriedBy == -1);
  const int dropped = story::find(w.events, EventType::UnitDropped);
  const int changed = story::find(w.events, EventType::SideChanged);
  CHECK(dropped >= 0 && changed > dropped);
  CHECK(w.events[(size_t)dropped].flags == kForced);

  // The same command on the carried unit itself.
  RescueWorld v = cast();
  CHECK(v.play(atTurn2(R"([{"changeSide":"lyra","side":"red"}])")));
  CHECK(v.act(Action::rescue(0, 1)));
  v.pass();
  CHECK_EQ(v.un(1).side, 1);
  CHECK(v.un(1).alive && v.un(0).carrying == -1 && v.un(0).side == 0);
}

TEST(rescue_story_change_side_leaves_both_units_alone_when_there_is_no_square) {
  // A 3 x 1 field: once red adds a soldier on the square the carried unit left, nothing is free.
  RescueWorld w(RescueMode::Carry, {{"soldier", 0, 0, 0, false}, {"archer", 0, 1, 0, false}, {"soldier", 1, 2, 0, false}},
                {"..."}, {"eirik", "lyra", "gorm"});
  CHECK(w.play(R"([
    {"id":"fill","when":{"turn":1,"side":"red"},"do":[{"addUnit":{"type":"soldier","side":"red","x":1,"y":0}}]},
    {"id":"turn","when":{"turn":2,"side":"blue"},"do":[{"changeSide":"eirik","side":"red"}]}])"));
  CHECK(w.act(Action::rescue(0, 1)));
  w.pass();
  CHECK(w.fired("fill") && w.fired("turn"));
  CHECK(w.battle.unitAt(w.at(1, 0)) == 3);
  CHECK_EQ(w.un(0).side, 0);
  CHECK(w.carriedBy(1, 0));
  CHECK_EQ(w.count(EventType::SideChanged), 0);
  CHECK_EQ(w.count(EventType::UnitDropped), 0);
}

TEST(rescue_story_give_item_skips_a_carried_unit) {
  RescueWorld w = cast();
  fixture::addItems(w);
  CHECK(w.play(atTurn2(R"([{"giveItem":"potion","to":{"side":"blue"}}])")));
  CHECK(w.act(Action::rescue(0, 1)));
  w.pass();
  CHECK_EQ(w.un(1).items.size(), (size_t)0);
  CHECK_EQ(w.un(0).items.size(), (size_t)1);
  CHECK(w.carriedBy(1, 0));
}

TEST(rescue_story_reach_objectives_only_see_units_on_squares) {
  RescueWorld w = cast();
  CHECK(w.play("", R"({"win":[{"reach":{"x":3,"y":1},"by":{"unit":"lyra"}}]})"));
  CHECK(w.act(Action::rescue(0, 1)));
  w.pass();
  CHECK(w.act(Action::move(0, w.at(2, 1))));  // Eirik carries Lyra next to the square: she is not on any square
  CHECK(!w.battle.over);
  CHECK(w.act(Action::drop(0, w.at(3, 1))));  // set down on it: she arrived
  CHECK(w.battle.over && w.battle.winnerTeam == 0);
}
