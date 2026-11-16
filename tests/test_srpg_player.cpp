// The player's strategy-RPG skeleton (docs/SRPG.md, step U0): the pick list behind the unit / square targeting
// modes, the menu id ranges, the unit details frame, and the battle view playing the eleven new events as no-ops.
// All headless, with fakes (the scene itself needs a window; the python player tests cover it).
#include "battle/battle_view.h"
#include "battle/srpg_menu_ids.h"
#include "battle/srpg_pick.h"
#include "battle/unit_info.h"
#include "battle_fixture.h"
#include "fake_renderer.h"

using namespace ote;

namespace {

InputEvent press(Button b) {
  InputEvent e;
  e.type = InputEvent::ButtonDown;
  e.button = b;
  return e;
}

}  // namespace

TEST(srpg_pick_cycles_wraps_chooses_and_cancels) {
  PickList p;
  p.begin({4, 7, 9});
  CHECK_EQ(p.current(), 4);
  CHECK_EQ(p.onButton(Button::Right), PickList::Moved);
  CHECK_EQ(p.current(), 7);
  CHECK_EQ(p.onButton(Button::Next), PickList::Moved);
  CHECK_EQ(p.onButton(Button::Down), PickList::Moved);  // wraps to the first
  CHECK_EQ(p.current(), 4);
  CHECK_EQ(p.onButton(Button::Left), PickList::Moved);  // and back to the last
  CHECK_EQ(p.current(), 9);
  CHECK_EQ(p.onButton(Button::Menu), PickList::None);
  CHECK_EQ(p.onButton(Button::Confirm), PickList::Chosen);
  CHECK_EQ(p.onButton(Button::Cancel), PickList::Cancelled);
}

TEST(srpg_pick_prefers_a_candidate_and_taps_like_attack_targeting) {
  PickList p;
  p.begin({4, 7, 9}, 9);
  CHECK_EQ(p.current(), 9);
  CHECK_EQ(p.tap(4), PickList::Moved);   // first tap highlights
  CHECK_EQ(p.current(), 4);
  CHECK_EQ(p.tap(4), PickList::Chosen);  // second tap on the same one chooses
  CHECK_EQ(p.tap(5), PickList::None);    // not a candidate
}

TEST(srpg_pick_with_nothing_to_pick_cannot_choose) {
  PickList p;
  p.begin({});
  CHECK(p.empty());
  CHECK_EQ(p.current(), -1);
  CHECK_EQ(p.onButton(Button::Right), PickList::None);
  CHECK_EQ(p.onButton(Button::Confirm), PickList::None);
  CHECK_EQ(p.onButton(Button::Cancel), PickList::Cancelled);
}

TEST(srpg_menu_ids_never_collide_and_ranges_map_to_their_group) {
  CHECK(srpgGroupOf(kTalkBase) == SrpgGroup::Talk);
  CHECK(srpgGroupOf(kTalkBase + 5) == SrpgGroup::Talk);
  CHECK(srpgGroupOf(kAbilityBase) == SrpgGroup::Ability);
  CHECK(srpgGroupOf(kAbilityBase + 20) == SrpgGroup::Ability);
  CHECK(srpgGroupOf(kSupportBase + 3) == SrpgGroup::Support);
  CHECK(srpgGroupOf(kSrpgRangeEnd) == SrpgGroup::None);
  for (int id : {kRescue, kDrop, kGive, kTake, kPairUp, kSwitch}) CHECK(srpgGroupOf(id) == SrpgGroup::Rescue);
  CHECK(srpgGroupOf(kPromote) == SrpgGroup::Promote);
  CHECK(srpgGroupOf(kInfo) == SrpgGroup::Info);
  CHECK(srpgGroupOf(kUnitEntry) == SrpgGroup::None);  // the battle menu's own entry, not an action menu group
  CHECK(srpgGroupOf(1) == SrpgGroup::None);           // the old entries (Attack, Wait, ...) stay out of it
  CHECK(srpgGroupOf(20) == SrpgGroup::None);
  CHECK(kRescue > 99 && kUnitEntry < kTalkBase);      // above the old small ids, below the Talk range
}

TEST(srpg_unit_info_turns_pages_and_closes) {
  fixture::World w({"...."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false}});
  CHECK(w.begin());
  // This game has no skills and no inventory: the details have the Stats page only (U1); other steps add theirs.
  UnitInfoView v;
  v.open(w.battle, 0);
  CHECK(v.isOpen());
  CHECK_EQ(v.pageCount(), 1);
  CHECK(v.onInput(press(Button::Right)) == UnitInfoView::None);
  CHECK_EQ(v.page(), 0);
  ote_test::FakeRenderer r;
  v.draw(r, w.battle, 8);  // draws the frame and the page
  CHECK(v.onInput(press(Button::Cancel)) == UnitInfoView::Closed);
  CHECK(!v.isOpen());
  CHECK(v.onInput(press(Button::Cancel)) == UnitInfoView::None);  // closed: ignores input
}

TEST(srpg_unit_info_with_fake_pages_turns_with_left_right) {
  fixture::World w({"...."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false}});
  CHECK(w.begin());
  UnitInfoView v;
  v.open(w.battle, 0);
  int drawn = -1;
  UnitInfoPages pages;
  for (int i = 0; i < 3; ++i)
    pages.push_back({"P" + std::to_string(i), [&drawn, i](Renderer&, int, int, int, int, int) { drawn = i; }});
  v.openPages(0, pages);
  ote_test::FakeRenderer r;
  v.draw(r, w.battle, 8);
  CHECK_EQ(drawn, 0);
  v.onInput(press(Button::Right));
  v.draw(r, w.battle, 8);
  CHECK_EQ(drawn, 1);
  v.onInput(press(Button::Left));
  v.onInput(press(Button::Left));  // wraps to the last
  v.draw(r, w.battle, 8);
  CHECK_EQ(drawn, 2);
  CHECK_EQ(v.page(), 2);
  // A tap outside the box closes; a tap inside turns.
  InputEvent tap;
  tap.type = InputEvent::PointerDown;
  tap.x = r.width() / 2 + 20;
  tap.y = r.height() / 2;
  CHECK(v.onInput(tap) == UnitInfoView::None);
  CHECK_EQ(v.page(), 0);
  tap.x = 0;
  tap.y = 0;
  CHECK(v.onInput(tap) == UnitInfoView::Closed);
}

TEST(srpg_view_plays_the_events_nobody_shows_yet_as_no_ops_that_end_at_once) {  // (UnitPromoted is U2's: test_srpg_player_promotion.cpp)
  fixture::World w({"...."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false}});
  CHECK(w.begin());
  BattleView view;
  view.init(w.battle, 24, true);
  // (the four rescue / pair up entries are animated since step U4: tests/test_srpg_player_rescue.cpp)
  const EventType types[] = {EventType::SkillLearned, EventType::SkillActivated, EventType::UnitHurt,
                             EventType::AbilityUsed,
                             EventType::SupportPoints, EventType::SupportReached};
  std::vector<Event> events;
  for (EventType t : types) {
    Event e;
    e.type = t;
    e.a = 0;
    e.b = 1;
    e.c = 0;
    events.push_back(e);
  }
  view.push(events);
  // The skill entries (U1) play for a moment (a label); the others end at once. All of it ends by itself.
  for (int i = 0; i < 200 && (i == 0 || view.busy()); ++i) view.update(0.05, w.battle);
  CHECK(!view.busy());
  CHECK(view.banner.empty());
}

TEST(srpg_view_resync_does_not_draw_a_carried_unit) {
  fixture::World w({"...."}, {{"soldier", 0, 0, 0, false}, {"soldier", 0, 1, 0, false}});
  CHECK(w.begin());
  // What R1 will leave behind for a carried unit: not alive, no cell, still has hit points, carriedBy set.
  w.battle.units[1].alive = false;
  w.battle.units[1].cell = kNoCell;
  w.battle.units[1].carriedBy = 0;
  BattleView view;
  view.init(w.battle, 24, true);
  CHECK(!view.units[1].visible);
  CHECK(view.units[0].visible);
}
