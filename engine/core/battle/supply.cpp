// Resupply (docs/ECONOMY.md "Repair and supply"): own structures (rules.supply.structures) and supplier units
// refill fuel and ammunition. Structures and suppliers act at the start of their side's turn; a supplier can also
// use the Supply action, which ends its turn.
#include "core/battle/battle.h"

namespace ote {

void Battle::stockUnit(Unit& u) const {
  const UnitTypeDef& t = typeOf(u);
  u.fuel = t.fuel > 0 ? t.fuel : -1;
  u.ammo.clear();
  if (db->rules.items.carriedWeapons) return;  // carried weapons wear out through the item's "uses" instead
  bool any = false;
  for (int w : t.weapons) any = any || db->weapons[(size_t)w].ammo > 0;
  if (!any) return;
  for (int w : t.weapons) {
    int a = db->weapons[(size_t)w].ammo;
    u.ammo.push_back(a > 0 ? a : -1);
  }
}

bool Battle::needsSupply(const Unit& u) const {
  if (u.fuel >= 0 && u.fuel < typeOf(u).fuel) return true;
  const std::vector<int>& list = typeOf(u).weapons;
  for (size_t i = 0; i < u.ammo.size() && i < list.size(); ++i)
    if (u.ammo[i] >= 0 && u.ammo[i] < db->weapons[(size_t)list[i]].ammo) return true;
  return false;
}

void Battle::refill(int unit, std::vector<Event>& events) {
  Unit& u = units[(size_t)unit];
  if (!needsSupply(u)) return;
  stockUnit(u);
  Event e;
  e.type = EventType::UnitResupplied;
  e.a = unit;
  events.push_back(e);
}

void Battle::resupplyTurn(std::vector<Event>& events) {
  // Own structures first (free), then suppliers; units in index order, so the result is deterministic.
  if (db->rules.supply.structures) {
    for (size_t i = 0; i < units.size(); ++i) {
      const Unit& u = units[i];
      if (!u.alive || !ownsTurn(u)) continue;
      int si = structureAt[(size_t)u.cell];
      if (si >= 0 && structures[(size_t)si].owner == u.side) refill((int)i, events);
    }
  }
  std::vector<int> targets;
  for (size_t i = 0; i < units.size(); ++i) {
    const Unit& u = units[i];
    if (!u.alive || !ownsTurn(u) || !typeOf(u).supplies) continue;
    supplyTargetsFrom((int)i, u.cell, targets);
    for (int t : targets) refill(t, events);
  }
}

void Battle::supplyTargetsFrom(int unit, Cell from, std::vector<int>& out) const {
  out.clear();
  if (unit < 0 || unit >= (int)units.size() || !grid.valid(from)) return;
  const Unit& u = units[(size_t)unit];
  if (!u.alive || !typeOf(u).supplies) return;
  for (const Unit& t : units)
    if (t.alive && t.id != unit && t.side == u.side && grid.distance(t.cell, from) == 1 && needsSupply(t))
      out.push_back(t.id);
}

bool Battle::canSupply(int unit) const {
  if (!canAct(unit) || units[(size_t)unit].done) return false;
  std::vector<int> targets;
  supplyTargetsFrom(unit, units[(size_t)unit].cell, targets);
  return !targets.empty();
}

void Battle::applySupply(const Action& action, std::vector<Event>& events) {
  std::vector<int> targets;
  supplyTargetsFrom(action.unit, units[(size_t)action.unit].cell, targets);
  for (int t : targets) refill(t, events);
  endOrder(action.unit);
}

}  // namespace ote
