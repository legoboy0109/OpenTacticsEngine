// "Whole-number scaling" (the player setting): the logical picture is shown at the largest whole multiple k of its size that fits
// the output, centred, with a plain border around it. A fractional scale is what lets thin lines show between map tiles (see
// pixel_snap.h and docs/GRAPHICS.md "Seams at fractional output scales"); a whole multiple cannot, by construction.
// No SDL in here, so it is unit-tested (tests/test_whole_scale.cpp).
#pragma once

namespace ote {

struct WholeScaleRect {
  int k = 0;           // the whole multiple (>= 1); 0 = not possible: the output is smaller than 1x the logical size, or a size is not positive
  int x = 0, y = 0;    // top-left of the picture in output pixels (the leftover is split in two, the odd pixel goes to the right / bottom)
  int w = 0, h = 0;    // k * logical size
};

// The picture rectangle for a logical size in an output of outW x outH real device pixels. When k is 0 the caller falls back to
// fitting the picture to the window (today's letterbox) so that nothing is cut off.
inline WholeScaleRect wholeScaleRect(int logicalW, int logicalH, int outW, int outH) {
  WholeScaleRect r;
  if (logicalW < 1 || logicalH < 1 || outW < 1 || outH < 1) return r;
  const int kx = outW / logicalW, ky = outH / logicalH;
  const int k = kx < ky ? kx : ky;
  if (k < 1) return r;
  r.k = k;
  r.w = logicalW * k;
  r.h = logicalH * k;
  r.x = (outW - r.w) / 2;  // floors: with an odd leftover the extra pixel is on the right / bottom
  r.y = (outH - r.h) / 2;
  return r;
}

}  // namespace ote
