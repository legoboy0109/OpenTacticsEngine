// The desktop window's starting size: the largest whole multiple of the game's logical size that fits the display
// (docs/WINDOWS.md "Decisions"). No SDL in here, so it is unit-tested (tests/test_window_fit.cpp).
#pragma once

namespace ote {

// Room kept for the window's frame and title bar, which the usable bounds do not leave out (the window size SDL is
// given is the inside of the window).
constexpr int kWindowFrameW = 16, kWindowFrameH = 48;
constexpr int kMaxWindowScale = 16;  // the same limit as --scale

// The largest scale s (1..kMaxWindowScale) with logical * s plus the frame inside usableW x usableH; 1 when nothing
// fits or the display is unknown (usable size 0 or less).
int fitWindowScale(int logicalW, int logicalH, int usableW, int usableH);

}  // namespace ote
