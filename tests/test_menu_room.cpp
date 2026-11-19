// menuRoomAfter (engine/player/ui/text.*): what a popup menu row has left after its label. No window: the stand-in
// renderer measures every character as 6 pixels, so ">" is 6 and the "  " gap is 12.
#include "fake_renderer.h"
#include "test.h"
#include "ui/text.h"

using namespace ote;

TEST(menu_room_after_the_label_is_the_screen_less_the_menus_own_padding) {
  ote_test::FakeRenderer r;
  // 480 - 4 (screen margin) - (8 padding + 6 arrow + 4 + 2 border) - 12 gap = 444 before the label.
  CHECK_EQ(menuRoomAfter(r, 480, ""), 444);
  CHECK_EQ(menuRoomAfter(r, 480, "Heal"), 444 - 24);
  CHECK_EQ(menuRoomAfter(r, 160, "Heal"), 160 - 36 - 24);
}

TEST(menu_room_after_a_label_too_wide_is_negative) {
  ote_test::FakeRenderer r;
  CHECK(menuRoomAfter(r, 60, "Longsword of Dawn") < 0);
}
