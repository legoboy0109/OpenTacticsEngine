// Height rules (docs/SRPG.md "Height rules"): how far a unit may climb or drop in a step, and what height does to the reach of a weapon.
// Nothing here changes the battle and nothing uses randomness. A game without "rules.height" never gets past the first line of each function.
#include <algorithm>
#include <cstdlib>  // std::abs(int): not pulled in by <algorithm> on every toolchain (Emscripten)

#include "core/battle/battle.h"
#include "core/grid/area.h"

namespace ote {

void Battle::jumpOf(const Unit& u, int& up, int& down) const {
  const HeightRules& h = db->rules.height;
  up = down = kNoHeightLimit;
  if (!h.on || h.flies(typeOf(u).moveType)) return;
  const UnitTypeDef& t = typeOf(u);
  up = t.jumpUp >= 0 ? t.jumpUp : h.jumpUp;
  down = t.jumpDown >= 0 ? t.jumpDown : h.jumpDown;
}

int Battle::rangeBonusMost(int weapon) const {
  const HeightRules& h = db->rules.height;
  return h.on && h.rangeEvery > 0 && db->weapons[(size_t)weapon].maxRange > 1 ? h.rangeMax : 0;
}

int Battle::rangeBonusAt(int weapon, Cell from, Cell to) const {
  const int most = rangeBonusMost(weapon);
  if (most <= 0) return 0;
  const int above = heightAt(from) - heightAt(to);  // whole levels the shooter stands above the target
  if (above <= 0) return 0;
  return std::min(most, above / db->rules.height.rangeEvery);
}

bool Battle::weaponReaches(int weapon, Cell from, Cell to) const {
  const WeaponDef& w = db->weapons[(size_t)weapon];
  const int dist = grid.distance(from, to);
  if (dist < w.minRange || dist > w.maxRange + rangeBonusAt(weapon, from, to)) return false;
  if (!w.heightRange.any()) return true;
  const int up = heightAt(to) - heightAt(from);  // > 0: the target stands higher
  if (w.heightRange.up >= 0 && up > w.heightRange.up) return false;
  if (w.heightRange.down >= 0 && -up > w.heightRange.down) return false;
  return true;
}

const char* Battle::heightAimRefusal(int weapon, Cell from, Cell to) const {
  if (!db->rules.height.on || !grid.valid(from) || !grid.valid(to)) return nullptr;
  const WeaponDef& w = db->weapons[(size_t)weapon];
  const int dist = grid.distance(from, to);
  const int up = heightAt(to) - heightAt(from);
  // Only when the distance alone would have been fine (or the height bonus is what makes it fine): then height is the reason.
  if (dist >= w.minRange && dist <= w.maxRange + rangeBonusAt(weapon, from, to)) {
    if (w.heightRange.up >= 0 && up > w.heightRange.up) return "too high to reach";
    if (w.heightRange.down >= 0 && -up > w.heightRange.down) return "too low to hit";
    if (w.fire == FireMode::Line && db->rules.height.lineOfSight && highGroundBetween(from, to)) return "the high ground is in the way";
  } else if (dist > w.maxRange && dist <= w.maxRange + rangeBonusMost(weapon)) {
    return "out of range from this height";
  }
  return nullptr;
}

// A "fire": "line" shot runs along a straight line of squares; one of the squares between the shooter and the target that is higher than
// BOTH ends stops it (equal to the higher end does not: the shot grazes it). Whole levels, no angles.
bool Battle::highGroundBetween(Cell from, Cell to) const {
  int steps = 0;
  const int dir = straightDirection(grid, from, to, &steps);
  if (dir < 0) return false;
  const int top = std::max(heightAt(from), heightAt(to));
  Cell c = from;
  for (int k = 1; k < steps; ++k) {
    c = grid.neighbor(c, dir);
    if (c == kNoCell) return false;
    if (heightAt(c) > top) return true;
  }
  return false;
}

void Battle::areaCover(int weapon, Cell from, Cell target, std::vector<Cell>& out) const { areaCoverOf(db->weapons[(size_t)weapon].area, from, target, out); }

void Battle::areaCoverOf(const AreaDef& area, Cell from, Cell target, std::vector<Cell>& out) const {
  areaCells(grid, from, target, area, out);
  if (!db->rules.height.on || area.vertical < 0) return;
  const int level = heightAt(target);
  out.erase(std::remove_if(out.begin(), out.end(), [&](Cell c) { return std::abs(heightAt(c) - level) > area.vertical; }), out.end());
}

const char* Battle::heightMoveRefusal(int unit, Cell cell) const {
  if (!db->rules.height.on || unit < 0 || unit >= (int)units.size()) return nullptr;
  Reach with, without;
  reachFor(unit, -1, with);
  if (with.canStop(cell)) return nullptr;
  reachFor(unit, -1, without, true, true);  // as if the unit could climb anything
  return without.canStop(cell) ? "too steep to climb there" : nullptr;
}

}  // namespace ote
