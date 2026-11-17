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
const int kMaxStaffLightTurns = 10;

enum class StaffTarget { Ally, Self, Team, Foe, Any };
enum class StaffArea { Single, Radius, All };

// One entry of "effects". Stage 1: Heal. Stage 2 (docs/SRPG.md section 8): the three that move the picked unit, the one
// that repairs an item it carries, and the one that lights a square. Later stages add kinds at the end.
struct StaffEffect {
  enum class Kind { Heal, Warp, Pull, Swap, Repair, Light, Inflict, Cure, Unlock };  // Inflict, Cure: stage 3 (conditions); Unlock: doors (docs/SRPG.md section 10)
  enum class Amount { Fixed, Full, Percent, Formula };
  Kind kind = Kind::Heal;
  Amount amount = Amount::Fixed;  // Heal: any; Repair: Fixed or Full
  int value = 0;  // Fixed: HP (Heal) or uses (Repair); Percent: share of the receiver's maximum
  int expr = -1;  // Formula: index into Database::exprs
  // Warp: how far from the USER the destination may be (the forms of the staff's own "range").
  int warpMin = 1, warpMax = 1;
  int warpExpr = -1;  // formula: the maximum (minimum 1)
  // Light: the squares within `radius` of the picked square are seen for `turns` of the user's side's turns.
  int radius = 0, turns = 1;
  // Light: the radius goes down by `shrink` squares each time the side's turn starts, until it is gone (0 = it does not shrink; docs/SRPG.md "Skill blocks, third set").
  int shrink = 0;
  // Inflict: `conds[0]` is the condition and `turns` (above, 0 = its own duration) overrides how long it lasts. Cure: the conditions
  // it removes, or all of them.
  std::vector<int> conds;
  bool cureAll = false;
  // Unlock: the kinds of structure it opens from afar (a door, a chest); the picked square is one of the structure's.
  bool unlockDoors = false, unlockChests = false;
  bool movesUnit() const { return kind == Kind::Warp || kind == Kind::Pull || kind == Kind::Swap; }
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
  int xpMiss = -1;                 // "xpMiss": experience when every unit it was used on was missed (-1 = the same as `xp`)
  int xpMissExpr = -1;             // formula instead of `xpMiss`
  std::vector<SkillCond> conds;    // "if", judged per unit hit ("who": "foe" = that unit)
  bool canMiss() const { return hitExpr >= 0; }
  // Nothing to pick: the user itself, or every unit in range.
  bool needsPick() const { return picksSquare() ? false : area != StaffArea::All && target != StaffTarget::Self; }
  bool has(StaffEffect::Kind k) const {
    for (const StaffEffect& e : effects)
      if (e.kind == k) return true;
    return false;
  }
  const StaffEffect* find(StaffEffect::Kind k) const {
    for (const StaffEffect& e : effects)
      if (e.kind == k) return &e;
    return nullptr;
  }
  bool movesUnit() const { return has(StaffEffect::Kind::Warp) || has(StaffEffect::Kind::Pull) || has(StaffEffect::Kind::Swap); }
  // A light staff and an unlock staff pick a SQUARE, not a unit (the loader allows them nothing else).
  bool picksSquare() const { return has(StaffEffect::Kind::Light) || has(StaffEffect::Kind::Unlock); }
};

// The plain reason a staff finds nobody in reach (a light has its own reasons, see Battle::staffOptions).
inline const char* staffNobodyWhy(const StaffDef& d) {
  if (d.has(StaffEffect::Kind::Heal)) return "nobody in reach needs that";
  if (d.movesUnit()) return "nobody in reach can be moved";
  if (d.has(StaffEffect::Kind::Cure)) return "nobody in reach has anything it cures";
  if (d.has(StaffEffect::Kind::Inflict)) return "nobody in reach can be affected";
  if (d.has(StaffEffect::Kind::Repair)) return "nobody in reach has an item to repair";
  if (d.has(StaffEffect::Kind::Unlock)) return "nothing in reach can be unlocked";
  return "nobody in reach needs that";
}

}  // namespace ote
