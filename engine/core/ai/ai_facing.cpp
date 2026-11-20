// Facing for the computer opponent (docs/AI.md "Facing"): the direction a unit looks in when it ends its turn without attacking.
#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

int endFacing(const Battle& b, const Unit& u) {
  if (!b.facingGame() || u.cell == kNoCell) return -1;
  int nearest = -1, best = 0;
  for (const Unit& o : b.units) {
    if (!knownEnemy(b, u.side, o) || o.cell == kNoCell) continue;  // fog: only what its team sees
    const int d = b.grid.distance(u.cell, o.cell);
    if (nearest < 0 || d < best) {  // strict: among equals the lowest unit id
      nearest = o.id;
      best = d;
    }
  }
  return nearest < 0 ? -1 : directionToward(b.grid, u.cell, b.units[(size_t)nearest].cell);
}

}  // namespace ai
}  // namespace ote
