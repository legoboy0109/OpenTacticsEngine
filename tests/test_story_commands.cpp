// Story event commands (docs/EVENTS.md "Commands"): what each does to the battle and which
// entries it leaves in the event stream.
#include "story_fixture.h"

using namespace ote;
using namespace story;
using namespace fixture::items;

namespace {

std::vector<std::string> field() { return {".........", ".........", "........."}; }
// A wall at 4,1.
std::vector<std::string> walled() { return {".........", "....#....", "........."}; }

const char* kAtStart = R"("when":{"battle":"start"})";

std::string startEvent(const std::string& commands) { return "[{\"id\":\"e\"," + std::string(kAtStart) + ",\"do\":" + commands + "}]"; }

}  // namespace

TEST(story_say_carries_speaker_portrait_and_text_by_index) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play(startEvent(R"([{"say":"Hello","speaker":"eirik","portrait":"eirik_calm"},
                              {"say":"Who goes?","speakerName":"Villager"},{"say":"Night falls."}])")));
  std::vector<const Event*> lines;
  for (const Event& e : w.events)
    if (e.type == EventType::Dialogue) lines.push_back(&e);
  CHECK_EQ(lines.size(), (size_t)3);
  if (lines.size() != 3) return;
  CHECK(lines[0]->a == 0 && lines[0]->c == -1);
  CHECK_STR(w.battle.storyText(lines[0]->b), "Hello");
  CHECK_STR(w.battle.storyText(lines[0]->d), "eirik_calm");
  CHECK(lines[1]->a == -1);
  CHECK_STR(w.battle.storyText(lines[1]->c), "Villager");
  CHECK(lines[2]->a == -1 && lines[2]->c == -1 && lines[2]->d == -1);
  CHECK_STR(w.battle.storyText(99), "");  // a bad index is harmless
}

TEST(story_add_unit_takes_the_nearest_free_square_it_can_stand_on) {
  StoryWorld w(walled(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play(startEvent(R"([
    {"addUnit":{"type":"archer","side":"red","x":6,"y":2,"id":"scout"}},
    {"addUnit":{"type":"soldier","side":"red","x":0,"y":1}},
    {"addUnit":{"type":"soldier","side":"red","x":0,"y":1}},
    {"addUnit":{"type":"soldier","side":"red","x":4,"y":1}}])")));
  const Battle& b = w.battle;
  CHECK_EQ((int)b.units.size(), 6);
  CHECK(b.units[2].cell == w.at(6, 2) && b.units[2].storyId == "scout");
  // Taken: the nearest free squares, ties going to the lowest cell number (above, then right, then below).
  CHECK(b.units[3].cell == w.at(0, 0));
  CHECK(b.units[4].cell == w.at(1, 1));
  CHECK(b.units[5].cell == w.at(4, 0));                    // a wall: the nearest square where it can stand
  CHECK_EQ(w.count(EventType::UnitAdded), 4);
  CHECK(b.occupant[(size_t)w.at(0, 0)] == 3 && b.units[5].alive && b.units[5].hp == 10);
  CHECK(w.unit("scout") == 2);
}

TEST(story_a_repeated_add_unit_does_not_duplicate_the_id) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play(R"([{"id":"e","when":{"turn":"every","side":"red"},"do":[
    {"addUnit":{"type":"soldier","side":"red","x":7,"y":0,"id":"extra"}}]}])"));
  CHECK(w.act(Action::endTurn()));
  CHECK(w.act(Action::endTurn()) && w.act(Action::endTurn()));
  CHECK_EQ(w.count(EventType::UnitAdded), 2);
  CHECK(w.battle.units[2].storyId == "extra" && w.battle.units[3].storyId.empty());
}

TEST(story_remove_unit_is_not_a_defeat) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}, {"soldier", 1, 8, 2, false}},
               {"eirik", "gorm", "grunt"});
  fixture::addItems(w);
  fixture::carry(w, 1, {Potion}, 0);  // gorm would drop it for whoever defeats him
  CHECK(w.play(R"([
    {"id":"e","when":{"turn":1},"do":[{"removeUnit":{"unit":"gorm"}}]},
    {"id":"d","when":{"defeated":{"unit":"gorm"}},"do":[{"say":"defeated"}]}])"));
  CHECK_EQ(w.count(EventType::UnitRemoved), 1);
  const Event* e = w.last(EventType::UnitRemoved);
  CHECK(e && e->a == 1 && e->b == w.at(8, 1));
  CHECK_EQ(w.count(EventType::UnitDied), 0);
  CHECK_EQ(w.count(EventType::ItemGained), 0);
  CHECK(!w.battle.units[1].alive && w.battle.occupant[(size_t)w.at(8, 1)] == -1);
  CHECK(w.battle.units[0].items.empty());
  CHECK(!w.fired("d"));
  CHECK(!w.battle.over);
}

TEST(story_removing_a_whole_side_still_ends_the_battle_by_the_rules) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play(startEvent(R"([{"removeUnit":{"side":"red"}}])")));
  CHECK(w.battle.over && w.battle.winnerTeam == 0);
}

TEST(story_move_unit_puts_the_unit_there_or_next_to_it_and_counts_as_arriving) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play(R"([
    {"id":"m","when":{"battle":"start"},"do":[{"moveUnit":"eirik","x":5,"y":1},{"moveUnit":"gorm","x":5,"y":1}]},
    {"id":"arrived","when":{"reach":{"x":5,"y":1},"by":"eirik"},"do":[{"say":"here"}]}])"));
  const Battle& b = w.battle;
  CHECK(b.units[0].cell == w.at(5, 1));
  CHECK(b.units[1].cell == w.at(5, 0));                    // taken: the nearest free square, lowest number first
  CHECK(b.occupant[(size_t)w.at(8, 1)] == -1);
  CHECK_EQ(w.count(EventType::UnitRelocated), 2);
  const Event* e = w.last(EventType::UnitRelocated);
  CHECK(e && e->a == 1 && e->b == w.at(8, 1) && e->c == w.at(5, 0));
  CHECK(w.fired("arrived"));
  CHECK(!b.units[0].moved);                                // it was put there, it did not spend its move
}

TEST(story_change_side_recruits_units) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 1, 1, false}, {"archer", 1, 8, 1, false}},
               {"eirik", "gorm", "grunt"});
  CHECK(w.play(startEvent(R"([{"changeSide":{"type":"soldier"},"side":"blue"}])")));
  CHECK(w.battle.units[1].side == 0 && w.battle.units[2].side == 1);
  const Event* e = w.last(EventType::SideChanged);
  CHECK(e && e->a == 1 && e->b == 1 && e->c == 0);
  CHECK_EQ(w.count(EventType::SideChanged), 1);            // eirik already is blue
  CHECK(!w.act(Action::attack(0, 1, 0)));                  // allies now
}

TEST(story_give_item_follows_the_item_rules) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 0, 1, 1, false}, {"soldier", 1, 8, 1, false}},
               {"eirik", "lyra", "gorm"});
  fixture::addItems(w);
  w.db.rules.items.inventorySize = 1;
  w.db.rules.items.storage = ItemStorage::Anywhere;
  fixture::carry(w, 1, {Ring});
  CHECK(w.play(startEvent(R"([{"giveItem":"potion","to":{"side":"blue"}}])")));
  const Battle& b = w.battle;
  CHECK(b.units[0].items.size() == 1 && b.units[0].items[0].item == Potion && b.units[0].items[0].uses == 2);
  CHECK(b.units[1].items.size() == 1 && b.units[1].items[0].item == Ring);  // full
  CHECK(b.sides[0].storage.size() == 1 && b.sides[0].storage[0].item == Potion);  // so it went to the storage
  CHECK_EQ(w.count(EventType::ItemGained), 2);

  StoryWorld lost(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  fixture::addItems(lost);
  lost.db.rules.items.inventorySize = 1;
  fixture::carry(lost, 0, {Ring});
  CHECK(lost.play(startEvent(R"([{"giveItem":"potion","to":"eirik"}])")));  // no storage in this game
  CHECK_EQ(lost.count(EventType::ItemLost), 1);
  CHECK(lost.battle.units[0].items.size() == 1 && lost.battle.sides[0].storage.empty());
}

TEST(story_give_resource_can_be_negative_but_never_below_zero) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play(R"([
    {"id":"a","when":{"battle":"start"},"do":[{"giveResource":{"funds":500},"side":"blue"}]},
    {"id":"b","when":{"turn":1},"do":[{"giveResource":{"funds":-700},"side":"blue"},{"giveResource":{"funds":0},"side":"red"}]}])"));
  CHECK_EQ(w.battle.sides[0].resources[0], 0);
  CHECK_EQ(w.count(EventType::ResourceChanged), 2);        // +500, then -500 (not -700); a gift of 0 says nothing
  const Event* e = w.last(EventType::ResourceChanged);
  CHECK(e && e->a == 0 && e->c == -500 && e->d == 0);
}

TEST(story_switches_and_variables) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play(startEvent(R"([{"setSwitch":"saved","to":true},{"setSwitch":"alarm","to":true},{"setSwitch":"alarm","to":false},
    {"setVariable":"gold","to":7},{"addVariable":"gold","by":-10},{"setVariable":"kills","to":999999999},
    {"addVariable":"kills","by":999999999}])")));
  CHECK(w.sw("saved") && !w.sw("alarm"));
  CHECK_EQ(w.var("gold"), -3);
  CHECK_EQ(w.var("kills"), 1000000000);                    // stays within what a save can hold
}

TEST(story_objective_command_replaces_the_objectives) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play(R"([{"id":"e","when":{"turn":2},"do":[{"objective":{"text":"Reach the gate","win":[{"reach":{"x":8,"y":1}}]}}]}])",
               R"({"text":"Hold the line","win":[{"round":9}]})"));
  CHECK_STR(w.battle.objectiveText(), "Hold the line");
  CHECK(w.act(Action::endTurn()) && w.act(Action::endTurn()));
  CHECK_STR(w.battle.objectiveText(), "Reach the gate");
  const Event* e = w.last(EventType::ObjectiveChanged);
  CHECK(e && w.battle.storyText(e->a) == "Reach the gate" && e->b == w.battle.storyState.objectives && e->b == 1);
}

TEST(story_win_and_lose_end_the_battle_for_a_team) {
  StoryWorld a(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(a.play(startEvent(R"([{"lose":"blue"}])")));
  CHECK(a.battle.over && a.battle.winnerTeam == 1);
  StoryWorld b(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(b.play(startEvent(R"([{"win":"red"},{"win":"blue"}])")));  // the first decision stands
  CHECK(b.battle.over && b.battle.winnerTeam == 1);
  CHECK_EQ(b.count(EventType::BattleEnded), 1);
}

TEST(story_if_then_else) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play(startEvent(R"([
    {"setVariable":"gold","to":3},
    {"if":[{"variable":"gold","op":">","value":5}],"then":[{"say":"rich"}],"else":[
       {"if":[{"switch":"saved"}],"then":[{"say":"saved"}],"else":[{"say":"poor"}]}]},
    {"if":[{"alive":"gorm"}],"then":[{"say":"gorm lives"}]}])")));
  std::vector<std::string> s = w.said();
  CHECK(s.size() == 2 && s[0] == ": poor" && s[1] == ": gorm lives");
}

TEST(story_focus_names_a_square_or_a_living_unit) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play(startEvent(R"([{"focus":{"x":3,"y":2}},{"focus":"gorm"},{"removeUnit":{"unit":"gorm"}},{"focus":"gorm"}])")));
  CHECK_EQ(w.count(EventType::Focus), 2);                  // nothing to follow once gorm is gone
  int first = find(w.events, EventType::Focus);
  CHECK(w.events[(size_t)first].a == w.at(3, 2) && w.events[(size_t)first].b == -1);
  const Event* e = w.last(EventType::Focus);
  CHECK(e && e->a == w.at(8, 1) && e->b == 1);
}

TEST(story_commands_leave_their_entries_in_the_order_they_ran) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play(startEvent(R"([{"say":"a"},{"addUnit":{"type":"soldier","side":"red","x":7,"y":1}},{"say":"b"},
    {"removeUnit":{"unit":"gorm"}}])")));
  std::vector<EventType> kinds;
  for (const Event& e : w.events) kinds.push_back(e.type);
  std::vector<EventType> want = {EventType::Dialogue, EventType::UnitAdded, EventType::Dialogue, EventType::UnitRemoved,
                                 EventType::TurnStarted};
  CHECK(kinds == want);
}
