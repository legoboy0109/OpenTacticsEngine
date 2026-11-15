// What the computer side knows (docs/ECONOMY.md "Fog of war: AI"). It plays by the same rules as a human: it reads
// enemy units, structure owners and capture state only through here, which answers from its team's sight and its
// memory of structure owners. Without fog every answer is the truth, so decisions are exactly what they were
// before fog existed. Nothing here uses randomness; ties go to the lowest index.
#include <algorithm>

#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

bool knownEnemy(const Battle& b, int side, const Unit& o) {
  const int team = b.sides[(size_t)side].team;
  return o.alive && b.teamOf(o) != team && b.unitVisibleTo(team, o);
}

int knownOwner(const Battle& b, int side, int structure) { return b.seenOwnerForSide(side, structure); }

bool knownCapturable(const Battle& b, const Unit& u, Cell at) {
  const int team = b.teamOf(u);
  const int si = b.structureIndexAt(at);
  if (!b.fogOn() || si < 0 || b.structureVisibleTo(team, si)) return b.canCaptureAt(u.id, at);  // in sight: the truth
  // Out of sight: judged on the owner the team last saw (the same test as Battle::canCaptureAt).
  if (!b.db->rules.capture || !u.alive || !b.typeOf(u).canCapture || b.defOf(b.structures[(size_t)si]).capturePoints <= 0)
    return false;
  const int owner = b.seenOwner(team, si);
  return owner < 0 || b.sides[(size_t)owner].team != team;
}

namespace {

// A hidden square as far as possible from everything the team has on the map: somewhere worth looking at.
Cell farthestUnseen(const Battle& b, int team) {
  std::vector<Cell> mine;
  for (const Unit& u : b.units)
    if (u.alive && b.teamOf(u) == team) mine.push_back(u.cell);
  Cell best = kNoCell;
  int bestDist = -1;
  for (Cell c = 0; c < b.grid.cellCount(); ++c) {
    if (b.visibleTo(team, c)) continue;
    int nearest = 1 << 20;
    for (Cell m : mine) nearest = std::min(nearest, b.grid.distance(c, m));
    if (nearest > bestDist) {  // strict: the lowest cell index wins a tie
      bestDist = nearest;
      best = c;
    }
  }
  return best;
}

}  // namespace

void enemyGoals(const Battle& b, int side, std::vector<Cell>& out) {
  out.clear();
  const int team = b.sides[(size_t)side].team;
  for (const Unit& u : b.units)
    if (knownEnemy(b, side, u)) out.push_back(u.cell);
  if (!out.empty() || !b.fogOn()) return;

  // Nobody in sight: head for the structures the team last saw in enemy hands (the enemy HQ is one of them), ...
  std::vector<Cell> footprint;
  for (size_t s = 0; s < b.structures.size(); ++s) {
    const int owner = b.seenOwner(team, (int)s);
    if (owner < 0 || b.sides[(size_t)owner].team == team) continue;
    b.structureCells((int)s, footprint);
    out.insert(out.end(), footprint.begin(), footprint.end());
  }
  if (!out.empty()) return;
  // ... else toward ground it cannot see.
  Cell c = farthestUnseen(b, team);
  if (c != kNoCell) out.push_back(c);
}

}  // namespace ai
}  // namespace ote
