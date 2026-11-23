// RowList (engine/player/ui/row_list.*) and the scrolling popup Menu (ui/menu.*): a list scrolls only when the selection
// would leave the visible rows or on purpose (wheel, "^" / "v" markers, a finger's drag); hovering never scrolls. No window:
// the stand-in renderer's lines are 8 pixels high.
#include "fake_renderer.h"
#include "test.h"
#include "ui/menu.h"
#include "ui/row_list.h"

using namespace ote;

namespace {

// 30 rows of 10 pixels, 8 visible, at (100, 50): rows 50..129, the "^" strip is 41..49 and the "v" strip 130..138.
RowList longList() {
  RowList l;
  l.x = 100;
  l.y = 50;
  l.w = 200;
  l.h = 80;
  l.rowH = 10;
  l.count = 30;
  return l;
}

InputEvent move(int x, int y, InputDevice d = InputDevice::Mouse) {
  InputEvent e;
  e.type = InputEvent::PointerMove;
  e.x = x;
  e.y = y;
  e.device = d;
  return e;
}

InputEvent press(InputEvent::Type t, int x, int y, InputDevice d = InputDevice::Mouse) {
  InputEvent e = move(x, y, d);
  e.type = t;
  return e;
}

InputEvent button(Button b) {
  InputEvent e;
  e.type = InputEvent::ButtonDown;
  e.button = b;
  return e;
}

// What a scene does with a pointer move: the list first, then select the row under the pointer.
void hover(RowList& l, int x, int y) {
  if (l.input(move(x, y))) return;
  const int row = l.rowAt(x, y);
  if (row >= 0) l.sel = row;
}

// `seconds` of frames of 1/60 s, reading the list each frame as drawing does.
void frames(RowList& l, double seconds) {
  for (double t = 0; t < seconds - 1e-9; t += 1.0 / 60) {
    advanceListClock(1.0 / 60);
    l.first();
  }
}

}  // namespace

TEST(row_list_hovering_near_the_bottom_never_scrolls) {
  RowList l = longList();
  CHECK_EQ(l.first(), 0);
  // The pointer creeps down over the bottom rows, many small moves, for a long time: the list stays where it is (the old rule
  // re-centred on every selected row and raced to the end).
  for (int i = 0; i < 200; ++i) {
    hover(l, 150, 110 + i % 20);
    frames(l, 1.0 / 60);
  }
  CHECK_EQ(l.first(), 0);
  CHECK(l.sel >= 6 && l.sel <= 7);
  // and the row drawn under the pointer is the row hit-testing finds there
  CHECK_EQ(l.rowY(l.rowAt(150, 125)), 120);
}

TEST(row_list_keys_keep_the_selection_in_view_and_centred) {
  RowList l = longList();
  for (int i = 0; i < 4; ++i) l.step(Button::Down);
  CHECK_EQ(l.sel, 4);
  CHECK_EQ(l.first(), 0);  // the middle is row 4 of 8: nothing to scroll yet
  for (int i = 0; i < 6; ++i) l.step(Button::Down);
  CHECK_EQ(l.sel, 10);
  CHECK_EQ(l.first(), 6);  // centred, as keys always did
  l.sel = 29;               // a jump the scene makes: brought into view
  CHECK_EQ(l.first(), 22);
  l.step(Button::Down);     // wraps to the top
  CHECK_EQ(l.sel, 0);
  CHECK_EQ(l.first(), 0);
  l.step(Button::Up);       // and back round to the end
  CHECK_EQ(l.sel, 29);
  CHECK_EQ(l.first(), 22);
}

TEST(row_list_a_hovered_row_after_keys_does_not_move_the_list) {
  RowList l = longList();
  for (int i = 0; i < 12; ++i) l.step(Button::Down);
  const int top = l.first();
  hover(l, 150, 125);  // the bottom row
  CHECK_EQ(l.sel, top + 7);
  CHECK_EQ(l.first(), top);
}

TEST(row_list_wheel_scrolls_one_row_a_notch_and_leaves_the_selection) {
  RowList l = longList();
  CHECK(l.input(button(Button::ZoomOut)));  // towards the player = down
  CHECK_EQ(l.first(), 1);
  l.input(button(Button::ZoomOut));
  l.input(button(Button::ZoomOut));
  CHECK_EQ(l.first(), 3);
  CHECK_EQ(l.sel, 0);  // scrolled out of view, not dragged along, and the list does not snap back to it
  CHECK_EQ(l.first(), 3);
  l.input(button(Button::ZoomIn));
  CHECK_EQ(l.first(), 2);
  for (int i = 0; i < 40; ++i) l.input(button(Button::ZoomOut));
  CHECK_EQ(l.first(), 22);  // stops at the end
  // a list that fits does not take the wheel (the map behind may zoom)
  RowList s = longList();
  s.count = 5;
  CHECK(!s.input(button(Button::ZoomOut)));
  CHECK_EQ(s.first(), 0);
}

TEST(row_list_marker_dwell_then_steady_rows_by_frame_time) {
  RowList l = longList();
  ote_test::FakeRenderer r;
  l.drawMore(r, Color(255, 255, 255));  // (the marker strips are a line high)
  CHECK_EQ(l.markerAt(150, 135), 1);
  CHECK_EQ(l.markerAt(150, 45), 0);  // no "^" at the top
  hover(l, 150, 135);                 // rest on "v"
  frames(l, 0.25);
  CHECK_EQ(l.first(), 0);  // still in the dwell
  frames(l, 0.1);          // 0.35 s: the dwell (0.3 s) is over, one row
  CHECK_EQ(l.first(), 1);
  frames(l, 1.0);          // 1.35 s: 1 + 6 rows a second for 1.05 s = 7
  CHECK_EQ(l.first(), 7);
  // the same with long frames (a slow machine): the rows due are the same
  RowList m = longList();
  m.drawMore(r, Color(255, 255, 255));
  hover(m, 150, 135);
  for (int i = 0; i < 9; ++i) advanceListClock(0.15);
  CHECK_EQ(m.first(), 7);
  // moving off the marker stops it at once; it does not go past the end
  hover(l, 150, 90);
  frames(l, 1.0);
  CHECK_EQ(l.first(), 7);
  hover(l, 150, 135);
  frames(l, 10.0);
  CHECK_EQ(l.first(), 22);
  // a key press lets the pointer go (no more marker scrolling until it moves again)
  hover(l, 150, 45);  // "^"
  l.input(button(Button::Left));
  frames(l, 2.0);
  CHECK_EQ(l.first(), 22);
}

TEST(row_list_a_click_on_a_marker_scrolls_one_row) {
  RowList l = longList();
  ote_test::FakeRenderer r;
  l.drawMore(r, Color(255, 255, 255));
  l.input(move(150, 134));
  CHECK(l.input(press(InputEvent::PointerDown, 150, 134)));
  CHECK_EQ(l.first(), 1);
  l.input(press(InputEvent::PointerUp, 150, 134));
  CHECK(l.input(press(InputEvent::PointerDown, 150, 44)));  // now there is a "^"
  CHECK_EQ(l.first(), 0);
  CHECK(!l.input(press(InputEvent::PointerDown, 150, 44)));  // and now there is not
}

TEST(row_list_a_finger_drags_the_rows_and_a_lifted_finger_scrolls_nothing) {
  RowList l = longList();
  ote_test::FakeRenderer r;
  l.drawMore(r, Color(255, 255, 255));
  CHECK(!l.input(press(InputEvent::PointerDown, 150, 120, InputDevice::Touch)));  // a tap stays a tap
  CHECK(l.input(move(150, 90, InputDevice::Touch)));                              // dragged up 30 px = 3 rows
  CHECK_EQ(l.first(), 3);
  l.input(press(InputEvent::PointerUp, 150, 90, InputDevice::Touch));
  // a finger lifted after a tap on "v": the list does not keep scrolling by itself
  l.input(press(InputEvent::PointerDown, 150, 134, InputDevice::Touch));
  l.input(press(InputEvent::PointerUp, 150, 134, InputDevice::Touch));
  const int top = l.first();
  frames(l, 2.0);
  CHECK_EQ(l.first(), top);
}

TEST(menu_taller_than_the_screen_scrolls_and_hover_does_not) {
  ote_test::FakeRenderer r;
  std::vector<MenuItem> items;
  for (int i = 0; i < 40; ++i) items.push_back(MenuItem("Item " + std::to_string(i), "", true, i + 1));
  Menu m;
  m.open(r, "Use", items, 10, 10, 480, 200);
  CHECK(m.h() <= 196);  // it fits the screen
  CHECK_EQ(m.firstShown(), 0);
  // hovering the lowest shown entry for a while scrolls nothing
  int x, y;
  m.rowCentre(0, x, y);
  int lastShown = 0;
  for (int i = 1; i < 40; ++i) {
    int cx, cy;
    m.rowCentre(i, cx, cy);
    if (cy < m.y() + m.h() - 12) lastShown = i;
  }
  m.rowCentre(lastShown, x, y);
  for (int i = 0; i < 100; ++i) {
    InputEvent e = move(x, y + i % 3);
    m.onInput(e);
    advanceListClock(1.0 / 60);
  }
  CHECK_EQ(m.firstShown(), 0);
  CHECK_EQ(m.selectedId(), lastShown + 1);
  // keys walk past the bottom and the menu scrolls with them; the wheel scrolls too
  for (int i = 0; i < 20; ++i) m.onInput(button(Button::Down));
  CHECK(m.firstShown() > 0);
  CHECK_EQ(m.selectedId(), lastShown + 21);
  const int top = m.firstShown();
  m.onInput(button(Button::ZoomIn));
  CHECK_EQ(m.firstShown(), top - 1);
  // a click on a shown row picks that row (hit-testing follows the scroll)
  m.rowCentre(top + 2, x, y);
  CHECK_EQ(m.onInput(press(InputEvent::PointerDown, x, y)), Menu::Chosen);
  CHECK_EQ(m.chosenId(), top + 3);
}

TEST(menu_that_fits_keeps_its_old_shape) {
  ote_test::FakeRenderer r;
  std::vector<MenuItem> items = {MenuItem("Attack"), MenuItem("Items"), MenuItem("Wait")};
  Menu m;
  m.open(r, "", items, 10, 10, 480, 270);
  CHECK_EQ(m.h(), 3 * 12 + 8 - 2);
  int x, y;
  m.rowCentre(1, x, y);
  CHECK_EQ(y, m.y() + 4 + 12 + 6);
}
