// Map weapons (docs/SRPG.md "Map weapons"): a structure kind may carry a weapon that a unit standing on it attacks with, next to what
// it carries. This file holds who may use it, how many uses are left and what a spent weapon does; the attack itself (forecast,
// strikes, experience) is the ordinary combat code with one extra weapon in the list (combat.cpp, combat_mods.cpp).
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

int Battle::mapWeaponOf(int structure) const {
  if (structure < 0 || structure >= (int)structures.size()) return -1;
  return defOf(structures[(size_t)structure]).mapWeapon.weapon;
}

int Battle::mapWeaponUses(int structure) const {
  if (structure < 0 || structure >= (int)structures.size()) return -2;
  const StructureState& st = structures[(size_t)structure];
  const MapWeaponDef& m = defOf(st).mapWeapon;
  if (!m.has()) return -2;
  return st.weaponFor == st.type ? st.weaponUses : m.uses;  // a kind that was just taken on starts with its own number
}

int Battle::mapWeaponStructure(const Unit& u, Cell cell, std::string* why) const {
  if (!u.alive || !grid.valid(cell)) return -1;
  const int si = structureAt[(size_t)cell];
  if (si < 0) return -1;
  const MapWeaponDef& m = defOf(structures[(size_t)si]).mapWeapon;
  if (!m.has()) return -1;
  auto no = [&](const std::string& reason) {
    if (why) *why = reason;
    return -1;
  };
  if (mapWeaponUses(si) == 0) return no("spent");
  if (enterCost(cell, typeOf(u).moveType) <= 0) return -1;  // (it could not stand there)
  if (m.restricted && std::find(m.byTypes.begin(), m.byTypes.end(), u.type) == m.byTypes.end()) {
    std::string who = db->unitTypes[(size_t)m.byTypes[0]].name;
    if (m.byTypes.size() == 2) who += " and " + db->unitTypes[(size_t)m.byTypes[1]].name;
    else if (m.byTypes.size() > 2) who += " and others";
    return no("Only " + who + " can use this");
  }
  if (weaponForbidden(u, m.weapon, why)) return -1;  // a condition that stops weapons (set `why` itself)
  const WeaponDef& w = db->weapons[(size_t)m.weapon];
  if (!m.ignoreRank && (!meetsRankFor(u, w, why) || !meetsLockFor(u, w, why))) return -1;
  return si;
}

void Battle::attackWeaponsAt(const Unit& u, Cell from, std::vector<int>& out, int* mapStructure) const {
  weaponsOf(u, out);
  if (mapStructure) *mapStructure = -1;
  const int si = mapWeaponStructure(u, from);
  if (si < 0) return;
  const int w = mapWeaponOf(si);
  if (std::find(out.begin(), out.end(), w) != out.end()) return;  // it carries that weapon itself: the carried one is used
  out.push_back(w);
  if (mapStructure) *mapStructure = si;
}

void Battle::spendMapWeapon(int structure, std::vector<Event>& events) {
  StructureState& st = structures[(size_t)structure];
  const MapWeaponDef& m = defOf(st).mapWeapon;
  const int left = mapWeaponUses(structure);
  if (left <= 0) return;  // unlimited, spent already, or no weapon
  st.weaponFor = st.type;
  st.weaponUses = left - 1;
  if (left - 1 > 0 || m.becomes < 0) return;
  // Spent: the kind it becomes (the map change's kind swap), and a unit left standing on squares it cannot enter there is moved.
  setStructureKind(structure, m.becomes, events);
  std::vector<Cell> footprint;
  structureCells(structure, footprint);
  changeTerrainCells({}, footprint, events);
}

}  // namespace ote
