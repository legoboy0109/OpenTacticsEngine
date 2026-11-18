#include "battle/weapon_choice.h"

#include <algorithm>

#include "battle/item_text.h"
#include "battle/mapweapon_text.h"
#include "ui/text.h"

namespace ote {
namespace {

std::string rangeText(const WeaponDef& w) {
  return "Range " + (w.minRange == w.maxRange ? intStr(w.minRange) : intStr(w.minRange) + "-" + intStr(w.maxRange));
}

// The numbers of the weapon against the first enemy that it reaches: the forecast's own, so the list never disagrees with the box
// that follows. Hit and Crit are left out when they are always 100 / 0 (a game without those formulas), as the forecast box does.
std::string statsText(const Battle& b, int unit, Cell from, const WeaponDef& w, int weapon, int target) {
  Forecast f;
  if (target < 0 || !b.forecast(unit, target, weapon, from, f)) return rangeText(w);
  std::string out = "Dmg " + intStr(f.attack.damage);
  if (f.attack.strikes > 1) out += " x" + intStr(f.attack.strikes);
  if (f.attack.hit < 100) out += "  Hit " + intStr(f.attack.hit) + "%";
  if (f.attack.crit > 0) out += "  Crit " + intStr(f.attack.crit) + "%";
  return out + "  " + rangeText(w);
}

}  // namespace

std::vector<WeaponRow> weaponRows(const Battle& b, int unit, Cell from, const std::vector<AttackOption>& options) {
  std::vector<WeaponRow> rows;
  if (unit < 0 || unit >= (int)b.units.size()) return rows;
  const Unit& u = b.units[(size_t)unit];
  const bool carried = b.db->rules.items.carriedWeapons;
  for (const AttackOption& o : options) {
    WeaponRow* row = nullptr;
    for (WeaponRow& r : rows)
      if (r.weapon == o.weapon) row = &r;
    if (row) {
      ++row->targets;
      continue;
    }
    rows.push_back(WeaponRow());
    WeaponRow& r = rows.back();
    const WeaponDef& w = b.db->weapons[(size_t)o.weapon];
    r.weapon = o.weapon;
    r.targets = 1;
    r.name = w.name;
    r.slot = carried ? b.weaponSlot(u, o.weapon) : -1;
    r.mapWeapon = o.mapWeapon && r.slot < 0;
    if (r.slot >= 0) {
      const ItemStack& s = u.items[(size_t)r.slot];
      r.name = b.itemOf(s).name;
      r.uses = usesText(b, s);
      r.equipped = r.slot == b.equippedSlot(u);
    } else if (r.mapWeapon) {
      const int structure = b.mapWeaponUsedBy(u, from, o.weapon);
      if (structure >= 0) r.name = mapWeaponForecastName(b, structure);
    } else {
      r.equipped = b.restingWeapon(u) == o.weapon;
      const std::vector<int>& own = b.typeOf(u).weapons;  // a fixed weapon with ammunition: what is left of what a supply gives
      for (size_t i = 0; i < own.size() && i < u.ammo.size(); ++i)
        if (own[i] == o.weapon && w.ammo > 0) r.uses = intStr(u.ammo[i]) + "/" + intStr(w.ammo);
    }
    r.stats = statsText(b, unit, from, w, o.weapon, o.target);
  }
  return rows;
}

std::vector<AttackOption> optionsOfWeapon(const std::vector<AttackOption>& options, int weapon) {
  std::vector<AttackOption> out;
  for (const AttackOption& o : options)
    if (o.weapon == weapon) out.push_back(o);
  return out;
}

int startRow(const std::vector<WeaponRow>& rows, const std::vector<AttackOption>& options, int prefer) {
  auto reaches = [&](const WeaponRow& r) {
    if (prefer < 0) return true;
    for (const AttackOption& o : options)
      if (o.weapon == r.weapon && o.target == prefer) return true;
    return false;
  };
  int firstReaching = -1, equipped = -1;
  for (size_t i = 0; i < rows.size(); ++i) {
    if (rows[i].equipped && equipped < 0) equipped = (int)i;
    if (reaches(rows[i]) && firstReaching < 0) firstReaching = (int)i;
  }
  if (equipped >= 0 && reaches(rows[(size_t)equipped])) return equipped;
  if (firstReaching >= 0) return firstReaching;
  return equipped >= 0 ? equipped : 0;
}

bool needsEquip(const Battle& b, int unit, const WeaponRow& row) {
  if (unit < 0 || unit >= (int)b.units.size() || row.mapWeapon || row.slot < 0) return false;
  return row.slot != b.equippedSlot(b.units[(size_t)unit]);
}

}  // namespace ote
