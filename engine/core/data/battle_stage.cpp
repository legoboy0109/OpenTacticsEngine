#include "core/data/battle_stage.h"

#include <algorithm>
#include <cmath>

namespace ote {

StageLayout stageLayoutFor(const BattleStage& st, int distance) {
  StageLayout l;
  const bool far = distance >= 2;
  const int(*feet)[2] = far && st.hasFeetFar ? st.feetFar : st.feet;
  const int(*plat)[2] = far && st.hasPlatformFar ? st.platformFar : st.platform;
  for (int s = 0; s < 2; ++s)
    for (int i = 0; i < 2; ++i) {
      l.feet[s][i] = feet[s][i];
      l.platform[s][i] = plat[s][i];
    }
  l.hasPlatform = st.hasPlatform;
  for (size_t i = 0; i < st.ranges.size(); ++i) {  // the band with the largest `from` that is not above the distance
    const StageBand& b = st.ranges[i];
    if (b.from > distance || (l.band >= 0 && st.ranges[(size_t)l.band].from >= b.from)) continue;
    l.band = (int)i;
  }
  if (l.band >= 0) {
    const StageBand& b = st.ranges[(size_t)l.band];
    for (int s = 0; s < 2; ++s)
      for (int i = 0; i < 2; ++i) {
        l.feet[s][i] = b.feet[s][i];
        l.platform[s][i] = b.platform[s][i];
      }
    l.hasPlatform = b.hasPlatform;
    l.each = b.each;
    l.panMs = b.panMs;
    l.leftView = b.leftView;
    l.rightView = b.rightView;
  }
  return l;
}

StageGeo stageGeometry(int W, int H, int hudBottom, int hintTop, const BattleStage& st, bool far, const int picW[2], const int picH[2]) {
  return stageGeometryAt(W, H, hudBottom, hintTop, st, far ? 2 : 1, picW, picH, 0);
}

StageGeo stageGeometryAt(int W, int H, int hudBottom, int hintTop, const BattleStage& st, int distance, const int picW[2], const int picH[2], double viewX) {
  StageGeo g;
  if (!st.on || st.width <= 0 || st.height <= 0 || W <= 0 || H <= 0) return g;
  g.valid = true;
  const int areaTop = std::max(0, std::min(hudBottom, H - 1)), areaBottom = std::max(areaTop + 1, std::min(hintTop, H));
  const int areaH = areaBottom - areaTop;
  int k = std::max(1, W / st.width);
  if (st.contain)
    while (k > 1 && st.height * k > areaH) --k;
  g.scale = k;
  g.w = st.width * k;
  g.h = st.height * k;
  g.x = (W - g.w) / 2;  // negative when the stage is wider than the screen: both sides are cropped

  const StageLayout lay = stageLayoutFor(st, distance);
  const int(*feet)[2] = lay.feet;
  const int(*plat)[2] = lay.platform;
  // The highest and lowest stage rows that must stay visible: the foot points and the platform pictures.
  int lowest = 0, highest = st.height;
  for (int s = 0; s < 2; ++s) {
    int top = feet[s][1], bottom = feet[s][1];
    const int ph = picH ? picH[s] : 0;
    if (ph > 0 && picW && picW[s] > 0) {
      const int py = lay.hasPlatform ? plat[s][1] : feet[s][1] - ph / 2;
      top = std::min(top, py);
      bottom = std::max(bottom, py + ph);
    }
    lowest = std::max(lowest, bottom);
    highest = std::min(highest, top);
  }

  if (g.h <= areaH) {
    g.y = areaTop + (areaH - g.h) / 2;  // fits between the boxes and the hint line: nothing hidden, nothing under them
  } else {
    // Taller than the area: it runs under the boxes and the hint, placed by `align` against the whole screen height
    // (cropped when it is taller than the screen too).
    switch (st.align) {
      case BattleStage::AlignTop: g.y = 0; break;
      case BattleStage::AlignBottom: g.y = H - g.h; break;
      default: g.y = (H - g.h) / 2; break;
    }
    // Feet and platforms always show inside the area: the least movement that does it (the bottom rule wins a conflict).
    g.y = std::max(g.y, areaTop - highest * k);
    g.y = std::min(g.y, areaBottom - lowest * k);
  }
  g.cropTop = g.y < 0 ? (-g.y) / k : 0;
  g.cropBottom = g.y + g.h > H ? (g.y + g.h - H) / k : 0;
  g.cropped = g.cropTop > 0 || g.cropBottom > 0 || g.x < 0;

  for (int s = 0; s < 2; ++s) {
    g.feet[s][0] = g.x + (int)std::lround((feet[s][0] - viewX) * k);
    g.feet[s][1] = g.y + feet[s][1] * k;
    const int pw = picW ? picW[s] : 0, ph = picH ? picH[s] : 0;
    if (pw > 0 && ph > 0) {
      g.platformW[s] = pw * k;
      g.platformH[s] = ph * k;
      if (lay.hasPlatform) {
        g.platform[s][0] = g.x + (int)std::lround((plat[s][0] - viewX) * k);
        g.platform[s][1] = g.y + plat[s][1] * k;
      } else {  // no declared place: centred on the foot point, like a scene without a stage
        g.platform[s][0] = g.feet[s][0] - g.platformW[s] / 2;
        g.platform[s][1] = g.feet[s][1] - g.platformH[s] / 2;
      }
    }
  }
  return g;
}

}  // namespace ote
