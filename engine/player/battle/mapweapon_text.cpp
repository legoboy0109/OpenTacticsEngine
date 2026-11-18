// The wording logic of mapweapon_text.h.
#include "battle/mapweapon_text.h"

#include <algorithm>

namespace ote {

namespace {

std::string num(int v) { return std::to_string(v); }

std::string rangeOf(const WeaponDef& w) { return w.minRange == w.maxRange ? num(w.minRange) : num(w.minRange) + "-" + num(w.maxRange); }

}  // namespace

std::string mapWeaponUsesText(const Battle& b, int structure) {
  const int left = b.mapWeaponUses(structure);
  if (left == -2 || left == -1) return "";
  if (left == 0) return MapWeaponWords::kSpent;
  const int most = std::max(left, b.defOf(b.structures[(size_t)structure]).mapWeapon.uses);
  return num(left) + "/" + num(most);
}

std::string mapWeaponRowValue(const Battle& b, int structure) {
  const int w = b.mapWeaponOf(structure);
  if (w < 0) return "";
  const std::string uses = mapWeaponUsesText(b, structure);
  return b.db->weapons[(size_t)w].name + (uses.empty() ? "" : " " + uses);
}

std::string mapWeaponForecastName(const Battle& b, int structure) {
  const int w = b.mapWeaponOf(structure);
  if (w < 0) return "";
  std::string text = b.db->weapons[(size_t)w].name;
  const int left = b.mapWeaponUses(structure);
  if (left > 0) text += " " + num(left);
  return text;
}

std::string mapWeaponForecastFrom(const Battle& b, int structure) {
  return std::string(MapWeaponWords::kFrom) + b.defOf(b.structures[(size_t)structure]).name;
}

bool mapWeaponUnitRow(const Battle& b, const Unit& u, std::string& label, std::string& value) {
  if (!u.alive || u.cell == kNoCell) return false;
  std::vector<int> own;
  int si = -1;
  b.attackWeaponsAt(u, u.cell, own, &si);
  if (si < 0) return false;
  const WeaponDef& w = b.db->weapons[(size_t)b.mapWeaponOf(si)];
  label = w.name;
  const std::string uses = mapWeaponUsesText(b, si);
  value = (uses.empty() ? "" : uses + " ") + "r" + rangeOf(w);
  return true;
}

std::string mapWeaponSpentFloat(const Battle& b, const Event& e) {
  std::string name = "Weapon";
  if (e.a >= 0 && e.a < (int)b.units.size() && b.units[(size_t)e.a].cell != kNoCell) {
    const int si = b.structureIndexAt(b.units[(size_t)e.a].cell);
    if (si >= 0) name = b.db->structures[(size_t)b.structures[(size_t)si].mapType].name;
  }
  return name + MapWeaponWords::kFloatSpent;
}

std::string mapWeaponLogLine(const Battle& b, const Event& e) {
  if (e.a < 0 || e.a >= (int)b.units.size() || e.c < 0 || e.c >= (int)b.db->weapons.size()) return "";
  const Unit& u = b.units[(size_t)e.a];
  std::string from = "a structure";
  // The unit stands where it stood (a unit that fell in the fight has no square: the structure is not named then).
  if (u.cell != kNoCell) {
    const int si = b.structureIndexAt(u.cell);
    if (si >= 0) from = b.db->structures[(size_t)b.structures[(size_t)si].mapType].name;
  }
  return "map weapon: " + u.name + " fires " + b.db->weapons[(size_t)e.c].name + " from " + from + ((e.flags & kMapWeaponSpent) ? " (" + std::string(MapWeaponWords::kSpent) + ")" : "");
}

}  // namespace ote
