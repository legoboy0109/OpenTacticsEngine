// Speech bubble placement, the dialogue box's bubble and notice shapes, and the name keyboard, without a window
// (engine/player/ui/bubble_layout.*, dialogue_shapes.cpp, name_entry.*; docs/EVENTS.md "Speech bubbles", "Notices", "Naming a character").
#include "fake_renderer.h"
#include "test.h"
#include "ui/bubble_layout.h"
#include "ui/dialogue_box.h"
#include "ui/name_entry.h"

using namespace ote;

namespace {

BubbleRequest request(int w = 120, int h = 30) {
  BubbleRequest q;
  q.areaW = 352;
  q.areaH = 259;
  q.bubbleW = w;
  q.bubbleH = h;
  return q;
}
BubbleAnchor at(int x, int y) {
  BubbleAnchor a;
  a.x = x;
  a.y = y;
  a.w = 24;
  a.h = 24;
  return a;
}
bool inside(const BubblePlace& p, const BubbleRequest& q) {
  return p.x >= q.areaX && p.y >= q.areaY && p.x + q.bubbleW <= q.areaX + q.areaW && p.y + q.bubbleH <= q.areaY + q.areaH;
}
InputEvent button(Button b) {
  InputEvent e;
  e.type = InputEvent::ButtonDown;
  e.button = b;
  return e;
}
InputEvent tap(int x, int y) {
  InputEvent e;
  e.type = InputEvent::PointerDown;
  e.x = x;
  e.y = y;
  return e;
}
DialogueLine line(const std::string& text, const std::string& name = "Eirik") {
  DialogueLine l;
  l.name = name;
  l.text = text;
  return l;
}

}  // namespace

// ---- placement

TEST(bubble_goes_above_the_speaker_when_it_fits) {
  const BubbleRequest q = request();
  const BubblePlace p = placeBubble(q, at(150, 120));
  CHECK(!p.below);
  CHECK_EQ(p.y + q.bubbleH + q.tail, 120);
  CHECK_EQ(p.x + q.bubbleW / 2, 162);  // centred over the speaker
  CHECK(p.hasTail() && p.tipY == 120 && p.tipX == 162);
  CHECK(!p.covers && inside(p, q));
}

TEST(bubble_goes_below_a_speaker_at_the_top) {
  const BubbleRequest q = request();
  const BubblePlace p = placeBubble(q, at(150, 4));
  CHECK(p.below);
  CHECK_EQ(p.y, 4 + 24 + q.tail);
  CHECK(p.hasTail() && p.tipY == 28);
  CHECK(!p.covers && inside(p, q));
}

TEST(bubble_slides_sideways_to_stay_in_the_view) {
  const BubbleRequest q = request(200, 30);
  const BubblePlace left = placeBubble(q, at(0, 120)), right = placeBubble(q, at(330, 120));
  CHECK_EQ(left.x, q.margin);
  CHECK_EQ(right.x + q.bubbleW, q.areaW - q.margin);
  CHECK(inside(left, q) && inside(right, q));
  // The tail still leaves the bubble over its own edge and points at the speaker.
  CHECK(left.baseX >= left.x && left.baseX < left.x + q.bubbleW && left.tipX == 12);
}

TEST(bubble_with_a_portrait_beside_it_keeps_both_in_the_view) {
  BubbleRequest q = request(150, 30);
  q.sideW = 48;
  q.sideH = 48;
  const BubblePlace p = placeBubble(q, at(0, 150));
  CHECK_EQ(p.sideX, q.margin);
  CHECK_EQ(p.x, p.sideX + 48 + q.gap);
  CHECK(!p.below && p.sideY + 48 + q.tail == 150);  // the taller portrait decides how high the block stands
  CHECK_EQ(p.y + q.bubbleH, p.sideY + 48);          // both stand on the block's bottom
  CHECK(!p.covers);
}

TEST(bubble_in_a_view_too_small_for_either_side_stays_inside_and_says_it_covers) {
  BubbleRequest q = request(120, 100);
  q.areaH = 150;
  const BubblePlace p = placeBubble(q, at(100, 60));
  CHECK(inside(p, q));
  CHECK(p.covers);
}

// ---- the box's shapes

TEST(dialogue_bubble_shape_is_laid_out_next_to_its_speaker) {
  ote_test::FakeRenderer r;
  DialogueBox box;
  DialogueFrame f;
  f.shape = DialogueFrame::Shape::Bubble;
  f.anchor = at(150, 150);
  f.areaW = 352;
  f.areaH = 259;
  f.portrait = 0;
  box.setFrame(f);
  box.open(r, 480, 270, 259, line("Hello there."), PortraitArt());
  CHECK(box.shape() == DialogueFrame::Shape::Bubble);
  CHECK(box.box().w < 352 && box.box().y + box.box().h < 150);  // narrower than the view, above the speaker
  CHECK(box.describeBubble().find("bubble: Eirik ") == 0);
  CHECK(box.describeBubble().find(" above tail 162,150") != std::string::npos);
  // It reads and ends like the box.
  CHECK(box.onInput(button(Button::Confirm)) == DialogueBox::None);
  CHECK(box.onInput(button(Button::Confirm)) == DialogueBox::Finished);
}

TEST(dialogue_bubble_follows_its_speaker) {
  ote_test::FakeRenderer r;
  DialogueBox box;
  DialogueFrame f;
  f.shape = DialogueFrame::Shape::Bubble;
  f.anchor = at(150, 150);
  f.areaW = 352;
  f.areaH = 259;
  box.setFrame(f);
  box.open(r, 480, 270, 259, line("Hello there."), PortraitArt());
  const int y0 = box.box().y;
  box.reanchor(at(150, 180));
  CHECK_EQ(box.box().y, y0 + 30);
}

TEST(dialogue_bubble_question_answers_stay_clear_of_the_bubble_and_start_on_the_cursor) {
  ote_test::FakeRenderer r;
  DialogueBox box;
  DialogueFrame f;
  f.shape = DialogueFrame::Shape::Bubble;
  f.anchor = at(150, 220);  // low: the bubble is above, the answers go to the top or bottom, never over it
  f.areaW = 352;
  f.areaH = 259;
  box.setFrame(f);
  box.open(r, 480, 270, 259, line("Which way?"), PortraitArt());
  box.setAnswers(r, {"North", "East", "Stay"}, 2);
  CHECK_EQ(box.highlighted(), 2);
  const DialogueBox::Rows p = box.panel(), b = box.box();
  CHECK(p.y + p.h <= b.y || p.y >= b.y + b.h);
  CHECK(p.y >= 0 && p.y + p.h <= 259);
}

TEST(dialogue_notice_is_centred_with_its_answers_inside) {
  ote_test::FakeRenderer r;
  DialogueBox box;
  DialogueFrame f;
  f.shape = DialogueFrame::Shape::Notice;
  box.setFrame(f);
  DialogueLine l = line("Repeat lesson?", "");
  l.notice = true;
  box.open(r, 480, 270, 259, l, PortraitArt());
  box.setAnswers(r, {"Yes", "No"}, 1);
  const DialogueBox::Rows b = box.box(), p = box.panel();
  CHECK_EQ(b.x + b.w / 2, 240);
  CHECK(p.y > b.y && p.y + p.h <= b.y + b.h);  // the answers sit inside the window
  CHECK_EQ(box.highlighted(), 1);
  CHECK(box.onInput(button(Button::Confirm)) == DialogueBox::None);   // the question is shown whole
  CHECK(box.onInput(button(Button::Confirm)) == DialogueBox::None);   // ... then its answers
  CHECK(box.choosing());
  CHECK(box.onInput(button(Button::Confirm)) == DialogueBox::Chosen);  // the cursor answer
  CHECK_EQ(box.chosen(), 1);
}

TEST(dialogue_box_shape_is_unchanged_without_a_frame) {
  ote_test::FakeRenderer r;
  DialogueBox a, b;
  a.open(r, 480, 270, 259, line("Hello there."), PortraitArt());
  b.setFrame(DialogueFrame());
  b.open(r, 480, 270, 259, line("Hello there."), PortraitArt());
  CHECK(a.shape() == DialogueFrame::Shape::Box);
  CHECK_EQ(a.box().x, b.box().x);
  CHECK_EQ(a.box().y, b.box().y);
  CHECK_EQ(a.box().w, b.box().w);
  CHECK_EQ(a.box().h, b.box().h);
  CHECK_EQ(a.box().x, 2);
  CHECK_EQ(a.box().w, 476);
}

// ---- the name keyboard

TEST(name_entry_types_deletes_resets_and_takes_ok) {
  ote_test::FakeRenderer r;
  NameEntry n;
  n.open(r, 480, 259, "Eirik", "Name the hero", "Eirik", 8);
  CHECK_STR(n.name(), "Eirik");
  for (int i = 0; i < 5; ++i) n.onInput(button(Button::Cancel));  // Cancel deletes
  CHECK_STR(n.name(), "");
  CHECK(n.onInput(tap(0, 0)) == NameEntry::None);
  const NameEntry::Key& k = n.keys()[(size_t)n.keyIndex("K")];
  n.onInput(tap(k.x + 1, k.y + 1));
  const NameEntry::Key& cs = n.keys()[(size_t)n.keyIndex("Case")];
  n.onInput(tap(cs.x + 1, cs.y + 1));
  CHECK(!n.upper() && n.keyIndex("a") >= 0);
  const NameEntry::Key& a = n.keys()[(size_t)n.keyIndex("a")];
  n.onInput(tap(a.x + 1, a.y + 1));
  CHECK_STR(n.name(), "Ka");
  // Menu jumps to OK, Confirm presses it.
  n.onInput(button(Button::Menu));
  CHECK_EQ(n.highlighted(), n.keyIndex("OK"));
  CHECK(n.onInput(button(Button::Confirm)) == NameEntry::Done);
  CHECK(!n.isOpen());
  CHECK_STR(n.name(), "Ka");
  // Reset goes back to the name it opened with.
  NameEntry m;
  m.open(r, 480, 259, "Eirik", "", "Eirik", 8);
  m.onInput(button(Button::Cancel));
  const NameEntry::Key& rs = m.keys()[(size_t)m.keyIndex("Reset")];
  m.onInput(tap(rs.x + 1, rs.y + 1));
  CHECK_STR(m.name(), "Eirik");
}

TEST(name_entry_keeps_to_its_most_letters_and_refuses_an_empty_name) {
  ote_test::FakeRenderer r;
  NameEntry n;
  n.open(r, 480, 259, "Eirik", "", "Abcdefghijkl", 3);
  CHECK_STR(n.name(), "Abc");  // a default longer than the limit is cut
  n.onInput(button(Button::Confirm));  // "A" pressed: full already
  CHECK_STR(n.name(), "Abc");
  for (int i = 0; i < 3; ++i) n.onInput(button(Button::Cancel));
  n.onInput(button(Button::Menu));
  CHECK(n.onInput(button(Button::Confirm)) == NameEntry::None);  // OK waits for a letter
  CHECK(n.isOpen());
}

TEST(name_entry_arrows_wrap_and_reach_the_wide_keys) {
  ote_test::FakeRenderer r;
  NameEntry n;
  n.open(r, 270, 470, "Eirik", "", "E", 8);  // a phone-shaped screen: the keys still fit
  for (const NameEntry::Key& k : n.keys()) CHECK(k.x >= 0 && k.x + k.w <= 270);
  CHECK_EQ(n.highlighted(), n.keyIndex("A"));
  n.onInput(button(Button::Left));  // wraps to the row's end
  CHECK_EQ(n.highlighted(), n.keyIndex("J"));
  n.onInput(button(Button::Up));  // wraps to the bottom row: the wide key under "J" is OK
  CHECK_EQ(n.highlighted(), n.keyIndex("OK"));
  n.onInput(button(Button::Down));  // and back to the top
  CHECK(n.highlighted() >= 0 && n.keys()[(size_t)n.highlighted()].y == n.keys()[0].y);
}
