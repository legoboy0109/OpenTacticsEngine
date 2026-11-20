// One physical press delivered by two sources must reach the game once (platform/common/press_dedupe.h).
#include "platform/common/press_dedupe.h"
#include "test.h"

using namespace ote;

namespace {
const PressSource kKey{PressSource::Keyboard, 0};
const PressSource kBack{PressSource::BackRequest, 0};
const PressSource kPad1{PressSource::Pad, 11};
const PressSource kPad2{PressSource::Pad, 12};
const PressSource kPad3{PressSource::Pad, 13};
}  // namespace

TEST(press_dedupe_pad_then_back_request_is_one_press) {  // the Xbox B case: the pad's event first, then AC_BACK
  PressDedupe d;
  CHECK(d.pass(Button::Cancel, kPad1, true, 1000));
  CHECK(!d.pass(Button::Cancel, kBack, true, 1004));   // second source while the first holds it
  CHECK(!d.pass(Button::Cancel, kBack, false, 1004));  // its release goes too
  CHECK(d.pass(Button::Cancel, kPad1, false, 1090));
  CHECK_EQ(d.swallowed(), 1);
}

TEST(press_dedupe_back_request_first_then_pad) {  // AC_BACK is pressed and released at once; the pad's B follows
  PressDedupe d;
  CHECK(d.pass(Button::Cancel, kBack, true, 2000));
  CHECK(d.pass(Button::Cancel, kBack, false, 2000));
  CHECK(!d.pass(Button::Cancel, kPad1, true, 2030));   // within the window after the first source's press
  CHECK(!d.pass(Button::Cancel, kPad1, false, 2120));
}

TEST(press_dedupe_one_pad_listed_three_times) {  // three instances of one physical pad
  PressDedupe d;
  CHECK(d.pass(Button::Confirm, kPad1, true, 10));
  CHECK(!d.pass(Button::Confirm, kPad2, true, 10));
  CHECK(!d.pass(Button::Confirm, kPad3, true, 10));
  CHECK(d.pass(Button::Confirm, kPad1, false, 90));
  CHECK(!d.pass(Button::Confirm, kPad2, false, 90));
  CHECK(!d.pass(Button::Confirm, kPad3, false, 90));
  CHECK_EQ(d.swallowed(), 2);
  // the next real press passes once more, from whichever copy reports first
  CHECK(d.pass(Button::Confirm, kPad2, true, 5000));
  CHECK(!d.pass(Button::Confirm, kPad1, true, 5000));
}

TEST(press_dedupe_release_order_does_not_matter) {
  PressDedupe d;
  CHECK(d.pass(Button::Up, kPad1, true, 0));
  CHECK(!d.pass(Button::Up, kPad2, true, 0));
  CHECK(!d.pass(Button::Up, kPad2, false, 50));  // the copy releases first
  CHECK(d.pass(Button::Up, kPad1, false, 60));
}

TEST(press_dedupe_second_source_after_the_window_is_a_new_press) {
  PressDedupe d;
  CHECK(d.pass(Button::Cancel, kKey, true, 0));
  CHECK(d.pass(Button::Cancel, kKey, false, 80));
  CHECK(!d.pass(Button::Cancel, kPad1, true, 399));   // still inside the window
  CHECK(!d.pass(Button::Cancel, kPad1, false, 450));
  CHECK(d.pass(Button::Cancel, kPad1, true, 481));    // 481 ms after the key's press: a new press
  CHECK(d.pass(Button::Cancel, kPad1, false, 520));
}

TEST(press_dedupe_a_held_button_blocks_a_second_source_however_long) {
  PressDedupe d;
  CHECK(d.pass(Button::Cancel, kPad1, true, 0));
  CHECK(!d.pass(Button::Cancel, kBack, true, 1500));  // still held, well past the window
  CHECK(d.pass(Button::Cancel, kPad1, false, 1600));
}

TEST(press_dedupe_a_lost_release_cannot_block_for_ever) {
  PressDedupe d;
  CHECK(d.pass(Button::Cancel, kPad1, true, 0));      // its release never comes
  CHECK(d.pass(Button::Cancel, kKey, true, PressDedupe::kHoldLimitMs + 1));
  d.reset();
  CHECK(d.pass(Button::Cancel, kPad2, true, 5));
  d.reset();
  CHECK(d.pass(Button::Cancel, kKey, true, 6));       // after reset nothing is held
}

TEST(press_dedupe_single_source_is_untouched) {  // keyboard-only or one-pad use: every event passes, repeats included
  PressDedupe d;
  for (int i = 0; i < 5; i++) {
    CHECK(d.pass(Button::Cancel, kKey, true, i * 30));      // quick taps, and Escape + Backspace together
    CHECK(d.pass(Button::Right, kKey, true, i * 30 + 1));   // direction auto-repeat (down, down, down ...)
    CHECK(d.pass(Button::Cancel, kKey, false, i * 30 + 10));
  }
  CHECK(d.pass(Button::Right, kKey, false, 200));
  CHECK(d.pass(Button::Confirm, kPad1, true, 300));
  CHECK(d.pass(Button::Confirm, kPad1, false, 310));
  CHECK(d.pass(Button::Confirm, kPad1, true, 320));   // a quick second tap on the same pad
  CHECK(d.pass(Button::Confirm, kPad1, false, 330));
  CHECK_EQ(d.swallowed(), 0);
}

TEST(press_dedupe_different_buttons_are_independent) {
  PressDedupe d;
  CHECK(d.pass(Button::Confirm, kPad1, true, 0));
  CHECK(d.pass(Button::Cancel, kBack, true, 1));      // another button from another source: not a duplicate
  CHECK(d.pass(Button::None, kPad2, true, 2));
}
