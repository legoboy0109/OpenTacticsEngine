#include "core/battle/threat_area.h"
#include "core/grid/area.h"

namespace ote {

void markWeaponReach(const Battle& b, int unit, int weapon, Cell s, std::vector<uint8_t>& attack) {
  const WeaponDef& w = b.db->weapons[(size_t)weapon];
  std::vector<Cell> around, covered;
  if (w.aimsStraight() || w.area.on() || b.heightGame()) {
    // Line fire, area weapons and the height rules (docs/SRPG.md "Area attacks and line fire", "Height rules"): the squares it can really be
    // aimed at from here (straight lines, stopped by the first unit or wall as they are today; range raised and limited by height), and every
    // square an aimed attack would cover. Units that may move or fall later change the picture, so this is what the board allows now.
    b.aimCells(weapon, s, unit, around);
    for (Cell c : around) {
      if (!w.area.on()) {
        attack[(size_t)c] = 1;
        continue;
      }
      b.areaCover(weapon, s, c, covered);
      for (Cell k : covered) attack[(size_t)k] = 1;
    }
    return;
  }
  b.grid.within(s, w.maxRange, around);
  for (Cell c : around)
    if (b.grid.distance(s, c) >= w.minRange) attack[(size_t)c] = 1;
}

void attackArea(const Battle& b, const Unit& u, const std::vector<Cell>& stops, std::vector<uint8_t>& attack) {
  attack.assign((size_t)b.grid.cellCount(), 0);
  std::vector<int> weapons;  // what it could use from there: carried weapons, and the weapon of a structure standing there
  for (Cell s : stops) {
    b.attackWeaponsAt(u, s, weapons);
    for (int wi : weapons) {
      if (s != u.cell && !b.db->weapons[(size_t)wi].afterMove) continue;
      markWeaponReach(b, u.id, wi, s, attack);
    }
  }
}

void threatArea(const Battle& b, int unit, int viewTeam, Reach& scratch, ThreatArea& out) {
  const size_t n = (size_t)b.grid.cellCount();
  out.move.assign(n, 0);
  out.attack.assign(n, 0);
  out.moveCount = out.attackOnlyCount = 0;
  if (unit < 0 || unit >= (int)b.units.size() || !b.units[(size_t)unit].alive) return;
  const Unit& u = b.units[(size_t)unit];
  b.reachFor(unit, viewTeam, scratch);
  for (Cell s : scratch.stops) out.move[(size_t)s] = 1;
  if (b.conditionBlock(u, ConditionBlock::Act) < 0) attackArea(b, u, scratch.stops, out.attack);
  for (size_t c = 0; c < n; ++c) {
    if (out.move[c]) ++out.moveCount;
    else if (out.attack[c]) ++out.attackOnlyCount;
  }
}

}  // namespace ote
