// Canvas placement on a TV screen (platform/common/screen_fit.h) and the Xbox video mode choice (platform/nxdk/video_pick.h).
#include "platform/common/screen_fit.h"
#include "platform/nxdk/video_pick.h"
#include "test.h"

using namespace ote;

namespace {
ScreenFitIn in(int sw, int sh, int pct, bool ana = false, bool integer = false) {
  ScreenFitIn i;
  i.screenW = sw;
  i.screenH = sh;
  i.safePercent = pct;
  i.anamorphic = ana;
  i.integerOnly = integer;
  return i;
}
}  // namespace

TEST(screen_fit_480_title_safe_is_width_limited_and_centred) {
  ScreenFit f = fitCanvas(in(640, 480, 90));  // 480x270 into 576x432: k = 1.2
  CHECK_EQ(f.w, 576);
  CHECK_EQ(f.h, 324);
  CHECK_EQ(f.x, 32);
  CHECK_EQ(f.y, 78);
  CHECK(f.kx > 1.19f && f.kx < 1.21f && f.kx == f.ky);
}

TEST(screen_fit_720p_fills_the_safe_box) {
  ScreenFit f = fitCanvas(in(1280, 720, 100));
  CHECK_EQ(f.w, 1280);
  CHECK_EQ(f.h, 720);
  CHECK_EQ(f.x, 0);
  f = fitCanvas(in(1280, 720, 95));
  CHECK_EQ(f.w, 1216);
  CHECK_EQ(f.h, 684);
}

TEST(screen_fit_integer_only_rounds_down_but_never_below_one) {
  ScreenFit f = fitCanvas(in(1280, 720, 100, false, true));  // 1280/480 = 2.67 -> 2
  CHECK_EQ(f.w, 960);
  CHECK_EQ(f.h, 540);
  f = fitCanvas(in(320, 240, 100, false, true));  // smaller than the canvas: best fit, not 0
  CHECK(f.w > 0 && f.w <= 320);
}

TEST(screen_fit_anamorphic_480_stretches_less_in_x) {
  ScreenFit f = fitCanvas(in(640, 480, 100, true));  // a 16:9 picture in a 4:3 pixel grid
  CHECK_EQ(f.w, 640);
  CHECK_EQ(f.h, 480);
  CHECK(f.kx < f.ky);
  f = fitCanvas(in(640, 480, 90, true));
  CHECK_EQ(f.w, 576);
  CHECK_EQ(f.h, 432);
}

TEST(screen_fit_bad_input_is_harmless) {
  ScreenFitIn i = in(640, 480, 10);  // clamped to 50
  ScreenFit f = fitCanvas(i);
  CHECK(f.w >= 1 && f.w <= 640);
  i.logicalW = 0;
  f = fitCanvas(i);
  CHECK_EQ(f.w, 640);
  CHECK_EQ(defaultSafePercent(false), 90);
  CHECK_EQ(defaultSafePercent(true), 95);
}

TEST(video_pick_takes_480_unless_720p_is_asked_for) {
  std::vector<VideoModeInfo> modes = {{640, 480}, {720, 480}, {1280, 720}};
  VideoWish w;
  CHECK_EQ(pickVideoMode(modes, w), 0);
  w.widescreen = true;  // a 16:9 TV still gets the 480 mode (stretched by the console)
  CHECK_EQ(pickVideoMode(modes, w), 0);
  w.preference = "480";
  CHECK_EQ(pickVideoMode(modes, w), 0);
  w.preference = "720p";
  CHECK_EQ(pickVideoMode(modes, w), 2);
}

TEST(video_pick_falls_back_to_what_is_offered) {
  VideoWish w;
  w.widescreen = true;
  w.preference = "720p";
  CHECK_EQ(pickVideoMode({{640, 480}, {720, 480}}, w), 0);  // 720p asked for but not offered (composite / S-video)
  CHECK_EQ(pickVideoMode({{1280, 720}}, VideoWish()), 0);   // only 720 offered
  CHECK_EQ(pickVideoMode({{720, 480}}, w), -1);             // nothing we draw well
  CHECK_EQ(pickVideoMode({}, w), -1);
}
