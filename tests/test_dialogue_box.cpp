// The dialogue box's behaviour without a window (engine/player/ui/dialogue_box.*): letters appear
// at a fixed speed, Confirm finishes a page then turns it then ends the line, holding Cancel /
// Menu or the Skip button skips everything, and the portrait changes the box's width.
#include "fake_renderer.h"
#include "test.h"
#include "ui/dialogue_box.h"

using namespace ote;

namespace {

InputEvent button(Button b, InputEvent::Type t = InputEvent::ButtonDown) {
  InputEvent e;
  e.type = t;
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

// Opens a box on the 480 x 270 fake screen (hint strip 11 px high).
DialogueBox open(const ote_test::FakeRenderer& r, const DialogueLine& l, const PortraitArt& art = PortraitArt()) {
  DialogueBox box;
  box.open(r, 480, 270, 259, l, art);
  return box;
}

// 6 px a letter, box 476 wide minus padding: about 75 letters a line; this is long enough for two pages.
std::string longText() {
  std::string t;
  for (int i = 0; i < 40; ++i) t += "word" + std::to_string(i % 10) + " ";
  return t;
}

}  // namespace

TEST(dialogue_box_confirm_finishes_the_page_then_the_line) {
  ote_test::FakeRenderer r;
  DialogueBox box = open(r, line("Hello there, friend."));
  CHECK(box.isOpen());
  CHECK_EQ(box.pageCount(), 1);
  // Nothing has been typed yet: the first Confirm shows the whole line, the second ends it.
  CHECK(box.onInput(button(Button::Confirm)) == DialogueBox::None);
  CHECK(box.onInput(button(Button::Confirm)) == DialogueBox::Finished);
}

TEST(dialogue_box_types_at_a_fixed_speed) {
  ote_test::FakeRenderer r;
  DialogueBox box = open(r, line("0123456789012345678901234567890123456789012345678901234567890123456789"));  // 70 letters
  // 60 letters a second: after half a second the line is not finished, so Confirm only completes it.
  box.update(0.5);
  CHECK(box.onInput(button(Button::Confirm)) == DialogueBox::None);
  CHECK(box.onInput(button(Button::Confirm)) == DialogueBox::Finished);
  // Left alone for 70 / 60 seconds the page finishes by itself: a single Confirm ends the line.
  DialogueBox slow = open(r, line("0123456789012345678901234567890123456789012345678901234567890123456789"));
  slow.update(70.0 / kDialogueLettersPerSecond + 0.05);
  CHECK(slow.onInput(button(Button::Confirm)) == DialogueBox::Finished);
}

TEST(dialogue_box_long_line_continues_on_a_second_page) {
  ote_test::FakeRenderer r;
  DialogueBox box = open(r, line(longText()));
  CHECK(box.pageCount() >= 2);
  CHECK(box.linesPerPage() >= 1 && box.linesPerPage() <= 3);
  CHECK_EQ(box.page(), 0);
  box.onInput(button(Button::Confirm));  // finish typing page 1
  CHECK(box.onInput(button(Button::Confirm)) == DialogueBox::None);  // turn the page
  CHECK_EQ(box.page(), 1);
  // Page 2 starts empty again and has to be typed.
  for (int i = 1; i < box.pageCount(); ++i) {
    box.onInput(button(Button::Confirm));
    if (i + 1 < box.pageCount()) CHECK(box.onInput(button(Button::Confirm)) == DialogueBox::None);
  }
  CHECK(box.onInput(button(Button::Confirm)) == DialogueBox::Finished);
}

TEST(dialogue_box_skip_by_hold_button_and_script) {
  ote_test::FakeRenderer r;
  // Holding Cancel for half a second skips everything; letting go earlier does not.
  DialogueBox box = open(r, line("Some text"));
  box.onInput(button(Button::Cancel));
  CHECK(box.update(kDialogueHoldToSkip * 0.5f) == DialogueBox::None);
  box.onInput(button(Button::Cancel, InputEvent::ButtonUp));
  CHECK(box.update(1.0) == DialogueBox::None);  // released: the timer is gone

  box.onInput(button(Button::Menu));  // Menu works too
  CHECK(box.update(kDialogueHoldToSkip * 0.6f) == DialogueBox::None);
  CHECK(box.update(kDialogueHoldToSkip * 0.6f) == DialogueBox::SkipAll);

  // A tap of Cancel alone does nothing (it is not "confirm").
  DialogueBox tapOnly = open(r, line("Some text"));
  CHECK(tapOnly.onInput(button(Button::Cancel)) == DialogueBox::None);
  CHECK(tapOnly.onInput(button(Button::Cancel, InputEvent::ButtonUp)) == DialogueBox::None);

  // The script's "skip".
  DialogueBox scripted = open(r, line("Some text"));
  CHECK(scripted.onInput(button(Button::Skip)) == DialogueBox::SkipAll);
}

TEST(dialogue_box_pointer_taps_advance_and_the_skip_button_skips) {
  ote_test::FakeRenderer r;
  DialogueBox box = open(r, line("Tap me"));
  // The Skip tab sits on the box's top-right corner, above the box.
  int skipX = 470, skipY = box.top() - 4;
  CHECK(box.onInput(tap(skipX, skipY)) == DialogueBox::SkipAll);
  // A tap anywhere else behaves like Confirm.
  CHECK(box.onInput(tap(100, 100)) == DialogueBox::None);
  CHECK(box.onInput(tap(100, 100)) == DialogueBox::Finished);
  // Other buttons (arrows, Tab) do nothing: the cursor and camera do not react.
  CHECK(box.onInput(button(Button::Left)) == DialogueBox::None);
  CHECK(box.onInput(button(Button::TurnLeft)) == DialogueBox::None);
}

TEST(dialogue_box_portrait_has_a_fixed_size_and_a_name_row_is_optional) {
  ote_test::FakeRenderer r;
  PortraitArt art;
  art.tex = 7;
  art.sw = 24;
  art.sh = 28;
  DialogueBox withFace = open(r, line("Hi"), art);
  DialogueBox noFace = open(r, line("Hi"));
  CHECK_EQ(noFace.portraitSize(), 0);
  CHECK_EQ(withFace.portraitSize(), 48);  // 48 px with the 8 px font
  // Same height either way; a line with no speaker has a shorter box (no name row).
  CHECK_EQ(withFace.top(), noFace.top());
  DialogueBox nameless = open(r, line("Hi", ""));
  CHECK(nameless.top() > noFace.top());
}
