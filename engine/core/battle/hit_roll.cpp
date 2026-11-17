// The "average2" hit roll (docs/SRPG.md "The hit roll"): the classic "true hit". Two numbers from the battle's own
// generator instead of one; a game with the default roll never gets here, so its random stream is untouched.
#include "core/battle/battle.h"

namespace ote {

bool Battle::rollHitAverage(int percent) {
  // Rolled in a fixed order (first, second) so a seed replays. floor((r1 + r2) / 2) < percent  <=>  r1 + r2 < 2 * percent.
  const int r1 = rng.range(100);
  const int r2 = rng.range(100);
  return r1 + r2 < 2 * percent;
}

}  // namespace ote
