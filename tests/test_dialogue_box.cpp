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

// ---- questions (docs/EVENTS.md "Dialogue choices") ----

namespace {

InputEvent move(int x, int y) {
  InputEvent e;
  e.type = InputEvent::PointerMove;
  e.x = x;
  e.y = y;
  return e;
}

// A question box on a screen of the given size (the hint strip is 11 px) with the answers up.
DialogueBox question(const ote_test::FakeRenderer& r, const std::vector<std::string>& answers, int w = 480, int h = 270) {
  DialogueBox box;
  box.open(r, w, h, h - 11, line("Which road do we take?"), PortraitArt());
  box.setAnswers(r, answers);
  return box;
}

// Reads the question: Confirm shows it whole, Confirm again brings up the answers.
void readQuestion(DialogueBox& box) {
  CHECK(box.onInput(button(Button::Confirm)) == DialogueBox::None);
  CHECK(!box.choosing());
  CHECK(box.onInput(button(Button::Confirm)) == DialogueBox::None);
  CHECK(box.choosing());
}

}  // namespace

TEST(dialogue_question_shows_its_answers_only_after_it_was_read) {
  ote_test::FakeRenderer r;
  DialogueBox box = question(r, {"The valley road", "The summit path"});
  CHECK(box.isQuestion() && !box.choosing() && box.answerCount() == 2);
  readQuestion(box);
  CHECK_EQ(box.highlighted(), 0);
  // The panel stands right above the question's box, inside the screen.
  DialogueBox::Rows p = box.panel();
  CHECK(p.y >= 0 && p.y + p.h <= box.top() && p.x >= 0 && p.x + p.w <= 480);
}

TEST(dialogue_question_keyboard_moves_wraps_and_confirms) {
  ote_test::FakeRenderer r;
  DialogueBox box = question(r, {"One", "Two", "Three"});
  readQuestion(box);
  CHECK(box.onInput(button(Button::Down)) == DialogueBox::None && box.highlighted() == 1);
  box.onInput(button(Button::Down));
  box.onInput(button(Button::Down));
  CHECK_EQ(box.highlighted(), 0);  // wraps past the last
  box.onInput(button(Button::Up));
  CHECK_EQ(box.highlighted(), 2);  // and back
  CHECK(box.onInput(button(Button::Confirm)) == DialogueBox::Chosen);
  CHECK_EQ(box.chosen(), 2);
}

TEST(dialogue_question_has_no_way_out) {
  ote_test::FakeRenderer r;
  DialogueBox box = question(r, {"One", "Two"});
  // Before the answers are up: Skip (button, script, tab) and holding Cancel / Menu only read the question at once.
  CHECK(box.onInput(button(Button::Skip)) == DialogueBox::None && box.choosing());
  DialogueBox held = question(r, {"One", "Two"});
  held.onInput(button(Button::Cancel));
  CHECK(held.update(kDialogueHoldToSkip + 0.1f) == DialogueBox::None && held.choosing());
  DialogueBox tab = question(r, {"One", "Two"});
  CHECK(tab.onInput(tap(470, tab.top() - 4)) == DialogueBox::None && tab.choosing());
  // With the answers up nothing but a pick ends it.
  for (Button b : {Button::Cancel, Button::Menu, Button::Skip, Button::Left, Button::Right})
    CHECK(box.onInput(button(b)) == DialogueBox::None);
  box.onInput(button(Button::Cancel));
  CHECK(box.update(5.0) == DialogueBox::None && box.choosing());
  CHECK(box.onInput(tap(5, 5)) == DialogueBox::None);  // a tap away from the answers
}

TEST(dialogue_question_pointer_hover_highlights_and_a_second_tap_picks) {
  ote_test::FakeRenderer r;
  DialogueBox box = question(r, {"One", "Two", "Three"});
  readQuestion(box);
  DialogueBox::Rows two = box.answerRow(1), three = box.answerRow(2);
  CHECK(box.onInput(move(two.x + 10, two.y + 2)) == DialogueBox::None && box.highlighted() == 1);
  // Hovered: one click picks (the mouse moved there first).
  CHECK(box.onInput(tap(two.x + 10, two.y + 2)) == DialogueBox::Chosen && box.chosen() == 1);
  // A touch screen has no hover: the first tap highlights, the second picks.
  DialogueBox touch = question(r, {"One", "Two", "Three"});
  readQuestion(touch);
  CHECK(touch.onInput(tap(three.x + 10, three.y + 2)) == DialogueBox::None && touch.highlighted() == 2);
  CHECK(touch.onInput(tap(three.x + 10, three.y + 2)) == DialogueBox::Chosen && touch.chosen() == 2);
}

TEST(dialogue_question_four_long_answers_fit_a_phone_sized_screen) {
  ote_test::FakeRenderer r;  // 6 x 8 px letters
  const std::string longA = "Take the old road through the marsh, even though everyone says that nobody ever comes back from it";
  const std::vector<std::string> four = {longA, "Wait here until the others have caught up with us and then decide together", longA + " twice", "No"};
  // Logical sizes of a phone held upright (the game may set any "view" size): 200 x 360, 240 x 426, 160 x 288.
  const int sizes[][2] = {{200, 360}, {240, 426}, {160, 288}, {480, 270}};
  for (const auto& sz : sizes) {
    DialogueBox box = question(r, four, sz[0], sz[1]);
    readQuestion(box);
    DialogueBox::Rows p = box.panel();
    CHECK(p.y >= 0 && p.y + p.h <= box.top() && p.x >= 0 && p.x + p.w <= sz[0]);  // the panel sits above the box, on the screen
    int prevEnd = p.y;
    for (int i = 0; i < 4; ++i) {
      DialogueBox::Rows row = box.answerRow(i);
      CHECK(box.answerLines(i) >= 1);
      CHECK(row.y >= prevEnd && row.y + row.h <= p.y + p.h);  // rows in order, inside the panel, not overlapping
      prevEnd = row.y + row.h;
    }
    if (sz[0] >= 480) CHECK(box.answerLines(0) <= 3);
  }
  // A short answer stays on one line.
  DialogueBox box = question(r, four, 200, 360);
  CHECK_EQ(box.answerLines(3), 1);
}

TEST(dialogue_a_line_that_is_not_a_question_is_unchanged_and_close_clears_the_answers) {
  ote_test::FakeRenderer r;
  DialogueBox box = question(r, {"One", "Two"});
  box.close();
  CHECK(!box.isQuestion() && !box.choosing());
  DialogueBox plain = open(r, line("No question here"));
  CHECK(!plain.isQuestion());
  plain.onInput(button(Button::Confirm));
  CHECK(plain.onInput(button(Button::Confirm)) == DialogueBox::Finished);
}
