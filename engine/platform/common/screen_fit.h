// Where the game's logical canvas goes on a fixed-size TV screen (the original Xbox, docs/XBOX_OG.md): scaled to the
// largest size that fits inside the title-safe rectangle, centred on the screen. No SDL in here, so it is unit-tested
// (tests/test_screen_fit.cpp). Desktop windows use SDL's own letterbox; only a backend that has a fixed video mode and
// a CRT-style overscan to respect calls this.
#pragma once

namespace ote {

struct ScreenFitIn {
  int logicalW = 480, logicalH = 270;
  int screenW = 640, screenH = 480;
  // Share of the screen's width and height that may be used, in percent (the rest is a black margin all around, for
  // the part of the picture an old TV hides). Clamped to 50..100.
  int safePercent = 100;
  // True when the video mode's pixels are 4/3 wider than they are high: a 640x480 frame that the TV shows as 16:9 (the
  // dashboard's widescreen setting on a 480 mode). The canvas is then stretched less in x than in y so it looks right.
  bool anamorphic = false;
  // Whole-number scale only (when at least 1.0 fits). Off = best fit (fractional).
  bool integerOnly = false;
};

struct ScreenFit {
  int x = 0, y = 0, w = 0, h = 0;  // the canvas rectangle in screen pixels
  float kx = 1, ky = 1;            // screen pixels per logical pixel, horizontally / vertically
};

ScreenFit fitCanvas(const ScreenFitIn& in);

// The default margin when the user has not chosen one, by kind of picture. Standard definition (480i/480p) goes to
// CRTs, which hide about 3.5% (action safe) to 5% (title safe) of every edge, sometimes more; the 90% is the
// title-safe rectangle of SMPTE RP 218 / EBU R95. HD sets (720p) usually overscan less and many show 1:1: 95%.
int defaultSafePercent(bool highDefinition);

}  // namespace ote
