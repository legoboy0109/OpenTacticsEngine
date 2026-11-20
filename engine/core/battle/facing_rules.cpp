// Facing (docs/SRPG.md "Facing"): the arc of an attack, a unit's default facing, and turning a unit (with the UnitFaced event the player animates).
// A game without "rules.facing" never gets past the first line of each function.
#include "core/battle/battle.h"
#include "core/grid/facing.h"

namespace ote {

int Battle::arcAt(Cell at, int facing, Cell from) const {
  if (!db->rules.facing.on || facing < 0) return kArcFront;
  return arcOf(grid, at, facing, from);
}

int Battle::defaultFacing(Cell cell, int team) const {
  if (!db->rules.facing.on) return -1;
  int nearest = -1, best = 0;
  for (const Unit& o : units) {
    if (!o.alive || o.cell == kNoCell || teamOf(o) == team) continue;
    const int d = grid.distance(cell, o.cell);
    if (nearest < 0 || d < best) {  // strict: among equals the lowest unit id (the list is in id order)
      nearest = o.id;
      best = d;
    }
  }
  const int toward = nearest >= 0 ? directionToward(grid, cell, units[(size_t)nearest].cell) : -1;
  return toward >= 0 ? toward : grid.directionCount() / 2;  // nobody to look at: south on squares
}

void Battle::assignFacings() {
  if (!db->rules.facing.on) return;
  for (Unit& u : units)
    if (u.facing < 0 && u.cell != kNoCell) u.facing = defaultFacing(u.cell, teamOf(u));  // (a unit off the board has none until it is put down)
}

void Battle::faceUnit(int unit, int dir, std::vector<Event>& events) {
  if (!db->rules.facing.on || dir < 0 || dir >= grid.directionCount()) return;
  Unit& u = units[(size_t)unit];
  if (u.facing == dir) return;
  Event e;
  e.type = EventType::UnitFaced;
  e.a = unit;
  e.b = dir;
  e.c = u.facing;
  u.facing = dir;
  events.push_back(e);
}

void Battle::faceToward(int unit, Cell target, std::vector<Event>& events) {
  if (!db->rules.facing.on) return;
  const int dir = directionToward(grid, units[(size_t)unit].cell, target);
  if (dir >= 0) faceUnit(unit, dir, events);
}

}  // namespace ote
