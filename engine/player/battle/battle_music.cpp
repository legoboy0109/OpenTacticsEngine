#include "battle/battle_music.h"

namespace ote {

int hostileUnitsLeft(const Battle& b, int side) {
  if (side < 0 || side >= (int)b.sides.size()) return -1;
  const int team = b.sides[(size_t)side].team;
  int n = 0;
  for (const Unit& u : b.units) {
    if (!u.alive || u.side < 0 || u.side >= (int)b.sides.size()) continue;
    if (b.sides[(size_t)u.side].team != team) ++n;
  }
  return n;
}

PhaseInfo phaseInfo(const Battle& b, int side) {
  PhaseInfo p;
  if (side < 0 || side >= (int)b.sides.size()) return p;
  p.side = b.sides[(size_t)side].id;
  p.ai = b.sides[(size_t)side].ai;
  p.hostiles = hostileUnitsLeft(b, side);
  return p;
}

}  // namespace ote
