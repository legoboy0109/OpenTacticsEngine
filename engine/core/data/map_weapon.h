// A structure kind's usable weapon (a ballista; docs/SRPG.md "Map weapons"). Plain data, no behaviour: the rules are in
// battle/mapweapon.cpp, the reading in data/loader_map_weapon.cpp.
#pragma once
#include <vector>

namespace ote {

struct MapWeaponDef {
  int item = -1;                // index into Database::items (a weapon item); -1 = this kind has no weapon
  int weapon = -1;              // that item's weapon (index into Database::weapons)
  int uses = -1;                // per placed structure; -1 = unlimited
  bool restricted = false;      // "by" was given: only the unit types in `byTypes` may use it
  std::vector<int> byTypes;     // unit type indices ("by": types and every type that has one of the "tags")
  bool ignoreRank = false;      // skip the weapon's own rank and character / class lock
  int becomes = -1;             // structure kind it turns into when its uses run out; -1 = stays
  bool has() const { return weapon >= 0; }
};

}  // namespace ote
