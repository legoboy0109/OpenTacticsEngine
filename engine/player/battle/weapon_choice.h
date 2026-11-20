// The weapon list of "attackWeaponChoice": "pick" (docs/SRPG.md "Menu options"): choosing Attack first asks which weapon to use. Free
// of SDL and of the scene so a unit test can drive it with a Battle alone. All placeholder wording of the list is here.
#pragma once
#include <string>
#include <vector>

#include "core/battle/battle.h"

namespace ote {

struct WeaponWords {
  static constexpr const char* kTitle = "Attack with";
  static constexpr const char* kHint = "{udok}:choose  {cancel}:back";
};

// One row of the weapon list.
struct WeaponRow {
  int weapon = -1;        // index into Database::weapons
  int slot = -1;          // inventory slot of the item that gives it (carried weapons); -1 = the type's own weapon or a structure's
  bool mapWeapon = false; // the weapon of the structure the unit stands on (never equipped)
  bool equipped = false;  // the weapon in hand now (the one the list starts on)
  int targets = 0;        // enemies it reaches from where the unit stands
  std::string name;       // the item's name (carried) or the weapon's
  std::string uses;       // "12/40" uses left, or the ammunition of a fixed weapon; empty without a limit
  std::string stats;      // "Dmg 7  Hit 90%  Crit 5%  Range 1-2" (the numbers against the first enemy it reaches); Hit / Crit only when they matter
};

// The weapons of `unit` that have at least one target from `from`, from the unit's attack options (Battle::attackOptions), in the
// order the unit carries them (a type's own weapons: in the type's order). Empty = nothing to attack.
std::vector<WeaponRow> weaponRows(const Battle& b, int unit, Cell from, const std::vector<AttackOption>& options);
// The attack options that use `weapon` (the targets of one row), in the options' order.
std::vector<AttackOption> optionsOfWeapon(const std::vector<AttackOption>& options, int weapon);
// The row the list starts on: the equipped weapon when it reaches `prefer` (an enemy the player tapped; -1 = any), else the first row
// that does, else the equipped one, else the first.
int startRow(const std::vector<WeaponRow>& rows, const std::vector<AttackOption>& options, int prefer);
// Does attacking with this row first need the Equip order (a carried weapon that is not the one in hand)?
bool needsEquip(const Battle& b, int unit, const WeaponRow& row);

}  // namespace ote
