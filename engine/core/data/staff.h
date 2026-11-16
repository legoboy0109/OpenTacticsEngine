// Staves (contract: docs/SRPG.md section 8): the data of an item of kind "staff". Included by database.h after
// srpg.h (it needs SkillCond and WeaponDef), so nothing here may include it.
//
// A staff is an item (ItemDef::kind == Staff, ItemDef::staff = an index into Database::staves) so uses, price,
// trading, storage, drops, rosters and saves are the item's own. What it shares with a weapon is `gear`: a WeaponDef
// that is NOT in Database::weapons and only carries the id, group, numbers (`props`) and the rank / lock fields, so
// Battle's rank and character-lock tests and the formulas' `w` serve it unchanged.
#pragma once
#include <string>
#include <vector>

namespace ote {

const int kMaxStaffRange = 20;
const int kMaxStaffRadius = 10;
const int kMaxStaffEffects = 4;
const int kMaxStaffXp = 1000;

enum class StaffTarget { Ally, Self, Team, Foe, Any };
enum class StaffArea { Single, Radius, All };

// One entry of "effects". Stage 1 has only the heal; later stages add kinds at the end (docs/SRPG.md "Stages 2 and 3").
struct StaffEffect {
  enum class Kind { Heal };
  enum class Amount { Fixed, Full, Percent, Formula };
  Kind kind = Kind::Heal;
  Amount amount = Amount::Fixed;
  int value = 0;  // Fixed: HP; Percent: share of the receiver's maximum
  int expr = -1;  // Formula: index into Database::exprs
};

struct StaffDef {
  WeaponDef gear;                  // id (the item's), group, props, rankGroup / rankNeed / rankXp, wielderIds / wielders
  int minRange = 1, maxRange = 1;  // fixed reach; the maximum is rangeExpr's value when that is set
  int rangeExpr = -1;              // formula: the maximum reach (minimum 1)
  StaffTarget target = StaffTarget::Ally;
  StaffArea area = StaffArea::Single;
  int radius = 0;                  // Area::Radius
  std::vector<StaffEffect> effects;
  int hitExpr = -1;                // percent chance per unit; -1 = always works (and no random number is spent)
  int xp = 0;                      // experience for the user per use
  int xpExpr = -1;                 // formula instead of `xp`
  std::vector<SkillCond> conds;    // "if", judged per unit hit ("who": "foe" = that unit)
  bool canMiss() const { return hitExpr >= 0; }
  // Nothing to pick: the user itself, or every unit in range.
  bool needsPick() const { return area != StaffArea::All && target != StaffTarget::Self; }
};

}  // namespace ote
