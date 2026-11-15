// Ammunition (docs/ECONOMY.md "Repair and supply"): fixed weapons with "ammo" spend one per attack.
// Unit::ammo has one entry per entry of the unit type's weapon list (-1 = unlimited), so two units of one type
// always line up. A weapon at 0 is simply not offered: weaponsOf (items.cpp) and counterWeapon (combat.cpp) skip it,
// and everything else (attack options, forecasts, the AI) goes through those.
#include "core/battle/battle.h"

namespace ote {

int Battle::ammoSlot(const Unit& u, int weapon) const {
  if (u.ammo.empty()) return -1;
  const std::vector<int>& list = typeOf(u).weapons;
  int first = -1;
  for (size_t i = 0; i < list.size() && i < u.ammo.size(); ++i) {
    if (list[i] != weapon || u.ammo[i] < 0) continue;
    if (u.ammo[i] > 0) return (int)i;  // a weapon listed twice: use up the first one that still has some
    if (first < 0) first = (int)i;
  }
  return first;
}

int Battle::ammo(int unit, int weapon) const {
  if (unit < 0 || unit >= (int)units.size()) return -1;
  const Unit& u = units[(size_t)unit];
  int slot = ammoSlot(u, weapon);
  return slot < 0 ? -1 : u.ammo[(size_t)slot];
}

int Battle::maxAmmo(int unit, int weapon) const {
  if (unit < 0 || unit >= (int)units.size() || weapon < 0 || weapon >= (int)db->weapons.size()) return -1;
  if (ammoSlot(units[(size_t)unit], weapon) < 0) return -1;
  return db->weapons[(size_t)weapon].ammo;
}

void Battle::spendAmmo(int unit, int weapon) {
  Unit& u = units[(size_t)unit];
  int slot = ammoSlot(u, weapon);
  if (slot >= 0 && u.ammo[(size_t)slot] > 0) --u.ammo[(size_t)slot];
}

}  // namespace ote
