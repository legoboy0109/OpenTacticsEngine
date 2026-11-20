#include "platform/common/screen_fit.h"

#include <cmath>

namespace ote {

ScreenFit fitCanvas(const ScreenFitIn& in) {
  ScreenFit f;
  if (in.logicalW < 1 || in.logicalH < 1 || in.screenW < 1 || in.screenH < 1) {
    f.w = in.screenW > 0 ? in.screenW : 0;
    f.h = in.screenH > 0 ? in.screenH : 0;
    return f;
  }
  int pct = in.safePercent < 50 ? 50 : in.safePercent > 100 ? 100 : in.safePercent;
  const float pa = in.anamorphic ? 4.0f / 3.0f : 1.0f;  // pixel width / pixel height
  const float boxW = (float)in.screenW * (float)pct / 100.0f * pa;  // the safe box in "height pixels"
  const float boxH = (float)in.screenH * (float)pct / 100.0f;
  float k = std::fmin(boxW / (float)in.logicalW, boxH / (float)in.logicalH);  // physical scale (y pixels per logical pixel)
  if (in.integerOnly && k >= 1.0f) k = std::floor(k);
  f.ky = k;
  f.kx = k / pa;
  f.w = (int)std::floor((float)in.logicalW * f.kx + 0.5f);
  f.h = (int)std::floor((float)in.logicalH * f.ky + 0.5f);
  if (f.w > in.screenW) f.w = in.screenW;
  if (f.h > in.screenH) f.h = in.screenH;
  if (f.w < 1) f.w = 1;
  if (f.h < 1) f.h = 1;
  f.x = (in.screenW - f.w) / 2;
  f.y = (in.screenH - f.h) / 2;
  return f;
}

int defaultSafePercent(bool highDefinition) { return highDefinition ? 95 : 90; }

}  // namespace ote
