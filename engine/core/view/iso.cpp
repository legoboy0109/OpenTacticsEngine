#include "core/view/iso.h"

#include <cmath>

namespace ote {

int IsoGeometry::pick(float wx, float wy, const std::vector<int>& elev) const {
  int best = -1, bestDepth = -1;
  // A square at elevation e fills the screen column of its diamond for every
  // level 0..e (the side faces below the top), so test each level's plane.
  for (int level = 0; level <= levels; ++level) {
    float gx, gy;
    unproject(wx, wy, (float)level, gx, gy);
    int x = (int)std::floor(gx), y = (int)std::floor(gy);
    if (x < 0 || y < 0 || x >= gridW || y >= gridH) continue;
    if (elev[(size_t)(y * gridW + x)] < level) continue;
    if (x + y > bestDepth) {
      bestDepth = x + y;
      best = y * gridW + x;
    }
  }
  return best;
}

bool IsoGeometry::hidden(int x, int y, const std::vector<int>& elev) const {
  if (step <= 0) return false;
  int own = elev[(size_t)(y * gridW + x)];
  // Squares straight below on screen: (x+k, y+k) is k diamonds lower. The two
  // squares beside it are half a diamond nearer but only reach the middle with
  // their corner, so they must rise as far as the one straight below.
  for (int k = 1; k <= gridW + gridH; ++k) {
    bool any = false;
    const int cand[3][3] = {{x + k, y + k - 1, 2 * k}, {x + k - 1, y + k, 2 * k}, {x + k, y + k, 2 * k}};
    for (const int* c : cand) {
      if (c[0] < 0 || c[1] < 0 || c[0] >= gridW || c[1] >= gridH) continue;
      any = true;
      int rise = elev[(size_t)(c[1] * gridW + c[0])] - own;
      if (rise > 0 && rise * step * 2 >= c[2] * tileH) return true;
    }
    if (!any || (levels - own) * step < k * tileH) break;
  }
  return false;
}

}  // namespace ote
