// The picture rectangle of the "Whole-number scaling" setting (platform/common/whole_scale.h).
#include "platform/common/whole_scale.h"
#include "test.h"

using namespace ote;

TEST(whole_scale_below_one_x_is_not_possible) {
  CHECK(wholeScaleRect(480, 270, 479, 270).k == 0);  // one pixel narrow
  CHECK(wholeScaleRect(480, 270, 480, 269).k == 0);  // one pixel short
  CHECK(wholeScaleRect(480, 270, 240, 135).k == 0);
  CHECK(wholeScaleRect(480, 270, 2000, 100).k == 0);  // wide enough but not tall enough
  CHECK(wholeScaleRect(480, 270, 0, 0).k == 0);
  CHECK(wholeScaleRect(0, 270, 480, 270).k == 0);
}

TEST(whole_scale_exact_multiples_fill_the_output) {
  for (int k = 1; k <= 8; ++k) {
    const WholeScaleRect r = wholeScaleRect(480, 270, 480 * k, 270 * k);
    CHECK(r.k == k && r.x == 0 && r.y == 0 && r.w == 480 * k && r.h == 270 * k);
  }
}

TEST(whole_scale_between_multiples_takes_the_smaller_one_and_centres) {
  // 1177x662 (the editor's frame at some sizes): 2.45x wide, 2.45x tall -> 2x = 960x540
  WholeScaleRect r = wholeScaleRect(480, 270, 1177, 662);
  CHECK(r.k == 2 && r.w == 960 && r.h == 540);
  CHECK(r.x == (1177 - 960) / 2 && r.y == (662 - 540) / 2);
  // 2560x1440 is 5.33x: 5x = 2400x1350 with a border
  r = wholeScaleRect(480, 270, 2560, 1440);
  CHECK(r.k == 5 && r.w == 2400 && r.h == 1350 && r.x == 80 && r.y == 45);
  // 1920x1080 is exactly 4x
  r = wholeScaleRect(480, 270, 1920, 1080);
  CHECK(r.k == 4 && r.x == 0 && r.y == 0);
}

TEST(whole_scale_odd_leftover_floors_the_offset) {
  // 1177 - 960 = 217: 108 left, 109 right.  662 - 540 = 122: 61 / 61.
  WholeScaleRect r = wholeScaleRect(480, 270, 1177, 662);
  CHECK(r.x == 108 && r.y == 61);
  CHECK(1177 - (r.x + r.w) == 109);
  r = wholeScaleRect(480, 270, 481, 271);  // 1x with one spare pixel each way: it goes right / down
  CHECK(r.k == 1 && r.x == 0 && r.y == 0);
}

TEST(whole_scale_the_tighter_axis_decides) {
  // wide window: height limits (2560x1401 -> 5x, 1401 - 1350 = 51 -> y 25)
  WholeScaleRect r = wholeScaleRect(480, 270, 2560, 1401);
  CHECK(r.k == 5 && r.x == 80 && r.y == 25);
  // tall window: width limits
  r = wholeScaleRect(480, 270, 1000, 3000);
  CHECK(r.k == 2 && r.x == 20 && r.y == (3000 - 540) / 2);
  // a non-uniform leftover: a big gap on one axis, none on the other
  r = wholeScaleRect(480, 270, 960, 5000);
  CHECK(r.k == 2 && r.x == 0 && r.y == (5000 - 540) / 2);
  // another logical size (a 320x240 game)
  r = wholeScaleRect(320, 240, 1000, 700);
  CHECK(r.k == 2 && r.w == 640 && r.h == 480 && r.x == 180 && r.y == 110);
}
