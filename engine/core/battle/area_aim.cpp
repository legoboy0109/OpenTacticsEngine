// Aiming: where a weapon can be fired from a square, with the straight-line rules of "fire": "line" and of a line-shaped area, and
// who an area attack hits (docs/SRPG.md "Area attacks and line fire"). Nothing here changes the battle.
#include <algorithm>

#include "core/battle/battle.h"
#include "core/grid/area.h"

namespace ote {

// What stops a shot besides units: terrain and structures that block sight (a wall, a closed door). The engine's one notion of this is
// "blocksSight" (docs/ECONOMY.md "Fog of war"); line fire honours it with or without fog in the map.
bool Battle::shotBlockedAt(Cell c) const {
  if (db->terrainBlocksSight && terrainAt(c).blocksSight) return true;
  const StructureDef* s = structureDefAt(c);
  return s && s->blocksSight;
}

bool Battle::weaponAims(int weapon, Cell from, Cell to, int ignoreUnit) const {
  if (!grid.valid(from) || !grid.valid(to)) return false;
  const WeaponDef& w = db->weapons[(size_t)weapon];
  if (!weaponInRange(weapon, grid.distance(from, to))) return false;
  if (!db->straightWeapons || !w.aimsStraight()) return true;  // the old rule: range only
  int steps = 0;
  if (straightDirection(grid, from, to, &steps) < 0) return false;
  if (w.fire != FireMode::Line) return true;  // an area "line" needs the straight line, but nothing blocks the aim
  // Line fire: every square before the target must be free, and the target square must not be a wall.
  const int dir = straightDirection(grid, from, to);
  Cell c = from;
  for (int k = 1; k < steps; ++k) {
    c = grid.neighbor(c, dir);
    if (c == kNoCell) return false;
    if (occupant[(size_t)c] >= 0 && occupant[(size_t)c] != ignoreUnit) return false;  // any unit stops the shot, a friend as well
    if (shotBlockedAt(c)) return false;
  }
  const int there = occupant[(size_t)to];
  return !(shotBlockedAt(to) && (there < 0 || there == ignoreUnit));  // a wall square cannot be aimed at, a unit standing in it can
}

void Battle::aimCells(int weapon, Cell from, int ignoreUnit, std::vector<Cell>& out) const {
  out.clear();
  if (!grid.valid(from)) return;
  const WeaponDef& w = db->weapons[(size_t)weapon];
  if (!db->straightWeapons || !w.aimsStraight()) {
    grid.within(from, w.maxRange, out);
    out.erase(std::remove_if(out.begin(), out.end(), [&](Cell c) { return !weaponInRange(weapon, grid.distance(from, c)); }), out.end());
    return;
  }
  for (int dir = 0; dir < grid.directionCount(); ++dir) {
    Cell c = from;
    for (int k = 1; k <= w.maxRange; ++k) {
      c = grid.neighbor(c, dir);
      if (c == kNoCell) break;
      const bool unit = occupant[(size_t)c] >= 0 && occupant[(size_t)c] != ignoreUnit;
      if (w.fire == FireMode::Line && shotBlockedAt(c) && !unit) break;  // a wall stops the shot before the square
      if (k >= w.minRange) out.push_back(c);
      if (w.fire == FireMode::Line && unit) break;  // the first unit stops it (it is the last square that can be aimed at)
    }
  }
  std::sort(out.begin(), out.end());
}

Cell Battle::aimedCell(const Action& action) const {
  if (action.cell != kNoCell) return grid.valid(action.cell) ? action.cell : kNoCell;
  if (action.target < 0 || (size_t)action.target >= units.size()) return kNoCell;
  return units[(size_t)action.target].cell;
}

bool Battle::areaHits(const Unit& attacker, int weapon, const Unit& u) const {
  if (!u.alive || u.id == attacker.id || u.cell == kNoCell) return false;
  const AreaDef& area = db->weapons[(size_t)weapon].area;
  // Foes always; a unit that is not a foe only when the weapon has "friendlyFire", or when the attacker is berserk (it strikes anyone).
  if (!isEnemy(attacker, u) && !area.friendlyFire && conditionBlock(attacker, ConditionBlock::Berserk) < 0) return false;
  double vs;
  return versusValue(weapon, u, vs);  // a weapon whose "versus" table leaves a unit out cannot hurt it
}

}  // namespace ote
