// The flash of changed squares (docs/EVENTS.md "Changing the map"): a white wash over each square that just changed terrain, fading out.
// A box per square works the same in every kind of view, like the flash of an upgraded structure.
#include <algorithm>

#include "battle/battle_scene.h"

namespace ote {

void BattleScene::drawMapChange(Renderer& r) {
  for (size_t c = 0; c < view_.cellFlash.size(); ++c) {
    const float f = view_.cellFlash[c];
    if (f <= 0) continue;
    int x, y, w, h;
    cellScreenBox((Cell)c, x, y, w, h);
    fillCellBox(r, x, y, w, h, Color(255, 255, 255, (uint8_t)(190 * std::min(1.0f, f))));
  }
}

}  // namespace ote
