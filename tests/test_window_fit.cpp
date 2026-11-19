// The desktop window's default scale: the largest that fits the display's usable area (platform/common/window_fit.h).
#include "platform/common/window_fit.h"
#include "test.h"

using namespace ote;

TEST(window_fit_picks_the_largest_scale_that_fits) {
  CHECK_EQ(fitWindowScale(480, 270, 1024, 728), 2);   // 1024x768 with a taskbar: 960x540, not 1440x810
  CHECK_EQ(fitWindowScale(480, 270, 1280, 680), 2);   // 1280x720 with a taskbar
  CHECK_EQ(fitWindowScale(480, 270, 1920, 1040), 3);  // 1920x1080 with a taskbar: 1440x810, as before
  CHECK_EQ(fitWindowScale(480, 270, 1920, 1080), 3);  // exactly 4x leaves no room for the frame
  CHECK_EQ(fitWindowScale(480, 270, 2560, 1400), 5);  // 2400x1350 and the frame
  CHECK_EQ(fitWindowScale(480, 270, 3840, 2120), 7);  // 4K
}

TEST(window_fit_never_goes_below_one_or_above_the_limit) {
  CHECK_EQ(fitWindowScale(480, 270, 800, 480), 1);  // 960 is too wide
  CHECK_EQ(fitWindowScale(480, 270, 320, 200), 1);  // smaller than the game itself: still 1
  CHECK_EQ(fitWindowScale(480, 270, 0, 0), 1);      // display unknown
  CHECK_EQ(fitWindowScale(480, 270, -1, 900), 1);
  CHECK_EQ(fitWindowScale(480, 270, 100000, 100000), kMaxWindowScale);
  CHECK_EQ(fitWindowScale(0, 270, 1920, 1080), 1);
}
