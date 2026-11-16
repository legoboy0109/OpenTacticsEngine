// The player's part of carrying and pair up (docs/SRPG.md step U4) that needs no window: what the action menu offers
// (rescue_menu.cpp) and how the battle view plays UnitRescued / UnitDropped / UnitHanded / PairSwitched
// (battle_view_rescue.cpp). The scene, the marker and the panel row are covered by tests/srpg_player_rescue_test.py.
#include <algorithm>

#include "battle/battle_view.h"
#include "battle/rescue_menu.h"
#include "battle/srpg_menu_ids.h"
#include "rescue_fixture.h"

using namespace ote;
using namespace rescuetest;

namespace {

std::vector<int> ids(const std::vector<RescueEntry>& entries) {
  std::vector<int> out;
  for (const RescueEntry& e : entries) out.push_back(e.id);
  return out;
}

// The events the world made after index `from` (what one action just pushed).
std::vector<Event> since(const RescueWorld& w, size_t from) { return std::vector<Event>(w.events.begin() + (long)from, w.events.end()); }

}  // namespace

TEST(rescue_menu_carry_offers_rescue_take_then_drop_give_in_the_contract_order) {
  RescueWorld w = line();
  CHECK(w.begin());
  CHECK(ids(rescueEntries(w.battle, 0)) == (std::vector<int>{kRescue}));  // nobody carries yet: nothing to take
  CHECK(w.act(Action::rescue(0, 1)));
  w.pass();
  CHECK(ids(rescueEntries(w.battle, 0)) == (std::vector<int>{kDrop, kGive}));
  CHECK(ids(rescueEntries(w.battle, 2)) == (std::vector<int>{kRescue, kTake}));  // beside the carrier
  CHECK(rescueEntries(w.battle, 0)[0].label == "Drop" && rescueEntries(w.battle, 0)[1].label == "Give");
  CHECK(rescuePickUnits(w.battle, 0, kGive) == (std::vector<int>{2}));
  CHECK(rescuePickUnits(w.battle, 2, kTake) == (std::vector<int>{0}));
  CHECK(!rescuePickCells(w.battle, 0).empty());
}

TEST(rescue_menu_a_drop_with_no_free_square_is_greyed_with_the_reason) {
  // Carrier 0 in the corner (0,0), the carried unit 1 at (1,0), unit 2 at (0,1); unit 3 steps into the freed square.
  RescueWorld w(RescueMode::Carry, {{"soldier", 0, 0, 0, false}, {"soldier", 0, 1, 0, false}, {"soldier", 0, 0, 1, false},
                                    {"soldier", 0, 1, 1, false}, {"soldier", 1, 2, 1, false}},
                {"...", "..."});
  CHECK(w.begin());
  CHECK(w.act(Action::rescue(0, 1)));
  w.pass();
  CHECK(w.act(Action::move(3, w.at(1, 0))));  // takes the square the carried unit left
  std::vector<RescueEntry> e = rescueEntries(w.battle, 0);
  CHECK(!e.empty() && e[0].id == kDrop);
  CHECK(!e[0].enabled && e[0].extra == "no room");
  CHECK(rescuePickCells(w.battle, 0).empty());
}

TEST(rescue_menu_pair_up_offers_pair_up_then_switch_and_drop) {
  RescueWorld w = line(RescueMode::PairUp);
  CHECK(w.begin());
  CHECK(ids(rescueEntries(w.battle, 1)) == (std::vector<int>{kPairUp}));
  CHECK(w.act(Action::pairUp(1, 0)));  // the archer goes behind the soldier
  w.pass();
  CHECK(ids(rescueEntries(w.battle, 0)) == (std::vector<int>{kSwitch, kDrop}));
  CHECK(rescueEntries(w.battle, 1).empty());  // the unit behind is out of the game
  CHECK(rescuePickUnits(w.battle, 2, kPairUp).size() == 1);  // the soldier beside it is the only partner (0 is taken)
}

TEST(rescue_menu_is_empty_without_the_rules_and_for_a_unit_that_acted) {
  fixture::World none({"...."}, {{"soldier", 0, 0, 0, false}, {"soldier", 0, 1, 0, false}});
  CHECK(none.begin());
  CHECK(rescueEntries(none.battle, 0).empty());
  RescueWorld w = line();
  CHECK(w.begin());
  w.battle.units[0].acted = true;
  CHECK(rescueEntries(w.battle, 0).empty());
  CHECK(rescueEntries(w.battle, -1).empty());
}

TEST(rescue_row_text_names_who_is_carried_or_with_whom) {
  RescueWorld w = line();
  CHECK(w.begin());
  CHECK(rescueRowText(w.battle, w.un(0), false).empty());
  CHECK(w.act(Action::rescue(0, 1)));
  CHECK(rescueRowText(w.battle, w.un(0), false) == "Carrying: " + w.un(1).name);
  CHECK(rescueRowText(w.battle, w.un(0), true) == "Carrying: someone");
  RescueWorld p = line(RescueMode::PairUp);
  CHECK(p.begin());
  CHECK(p.act(Action::pairUp(1, 0)));
  CHECK(rescueRowText(p.battle, p.un(0), false) == "With: " + p.un(1).name);
}

// ---- the view ----

TEST(rescue_view_slides_the_rescued_unit_onto_its_carrier_and_hides_it) {
  RescueWorld w = line();
  CHECK(w.begin());
  BattleView view;
  view.init(w.battle, 24, true);
  const size_t from = w.events.size();
  CHECK(w.act(Action::rescue(0, 1)));
  view.push(since(w, from));
  view.update(0.0, w.battle);
  CHECK(view.busy());
  CHECK(view.units[1].visible);
  CHECK_EQ((int)view.units[1].px, 48);  // starts on the square it left (2, 1)
  view.update(0.14, w.battle);
  CHECK(view.units[1].px > 24 && view.units[1].px < 48);  // half way to the carrier at (1, 1)
  view.update(1.0, w.battle);
  CHECK(!view.busy());
  CHECK(!view.units[1].visible);
  CHECK(view.units[0].visible);
}

TEST(rescue_view_slides_a_dropped_unit_out_of_its_carrier_onto_the_square) {
  RescueWorld w = line();
  CHECK(w.begin());
  CHECK(w.act(Action::rescue(0, 1)));
  w.pass();
  BattleView view;
  view.init(w.battle, 24, true);
  CHECK(!view.units[1].visible);
  const size_t from = w.events.size();
  CHECK(w.act(Action::drop(0, w.at(1, 0))));
  view.push(since(w, from));
  view.update(0.0, w.battle);
  CHECK(view.units[1].visible);
  CHECK_EQ((int)view.units[1].px, 24);  // out of the carrier at (1, 1)
  CHECK_EQ((int)view.units[1].py, 24);
  view.update(1.0, w.battle);
  CHECK(!view.busy());
  CHECK(view.units[1].visible);
  CHECK_EQ((int)view.units[1].px, 24);  // on (1, 0)
  CHECK_EQ((int)view.units[1].py, 0);
}

TEST(rescue_view_a_blocked_drop_shows_the_blocker_and_keeps_the_unit_carried) {
  RescueWorld w = line();
  CHECK(w.begin());
  CHECK(w.act(Action::rescue(0, 1)));
  BattleView view;
  view.init(w.battle, 24, true);
  view.units[4].visible = false;  // (what fog would do: the enemy is not drawn)
  Event e;
  e.type = EventType::UnitDropped;
  e.a = 0;
  e.b = 1;
  e.c = w.at(2, 1);
  e.d = 4;
  e.flags = kBlocked;
  view.push({e});
  view.update(0.0, w.battle);
  CHECK(view.units[4].visible);  // revealed
  bool said = false;
  for (const FloatText& f : view.floats) said |= f.text == "Blocked!";
  CHECK(said);
  view.update(2.0, w.battle);
  CHECK(!view.busy());
  CHECK(!view.units[1].visible);  // still carried: the rules say so
}

TEST(rescue_view_hands_the_unit_from_one_carrier_to_the_next) {
  RescueWorld w = line();
  CHECK(w.begin());
  CHECK(w.act(Action::rescue(0, 1)));
  w.pass();
  BattleView view;
  view.init(w.battle, 24, true);
  const size_t from = w.events.size();
  CHECK(w.act(Action::giveUnit(0, 2)));
  view.push(since(w, from));
  view.update(0.0, w.battle);
  CHECK(view.units[1].visible);
  view.update(0.14, w.battle);
  CHECK_EQ((int)view.units[1].px, 24);  // (1, 1) to (1, 2): the same column, half way down
  CHECK(view.units[1].py > 24 && view.units[1].py < 48);
  view.update(1.0, w.battle);
  CHECK(!view.busy());
  CHECK(!view.units[1].visible);
}

TEST(rescue_view_cross_fades_the_pair_on_a_switch) {
  RescueWorld w = line(RescueMode::PairUp);
  CHECK(w.begin());
  CHECK(w.act(Action::pairUp(1, 0)));
  w.pass();
  BattleView view;
  view.init(w.battle, 24, true);
  CHECK(view.units[0].visible && !view.units[1].visible);
  const size_t from = w.events.size();
  CHECK(w.act(Action::switchPair(0)));  // the archer comes to the front
  view.push(since(w, from));
  view.update(0.0, w.battle);
  CHECK(view.units[0].visible && view.units[1].visible);
  CHECK(view.units[0].alpha > 0.99f && view.units[1].alpha < 0.01f);
  view.update(0.15, w.battle);
  CHECK(view.units[0].alpha < 0.9f && view.units[1].alpha > 0.1f);
  view.update(1.0, w.battle);
  CHECK(!view.busy());
  CHECK(!view.units[0].visible && view.units[1].visible);
  CHECK(view.units[1].alpha > 0.99f);
}
