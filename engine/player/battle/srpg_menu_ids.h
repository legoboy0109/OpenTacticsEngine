// Menu entry ids of the strategy-RPG groups in the battle menus (docs/SRPG.md "Player UI", step U0).
// Free of any scene or SDL type so a unit test can use it.
//
// The ids of one action menu never collide: kAttack.. are the small enum of BattleScene (1..~25), the fixed ids
// below start at 100, Talk entries are kTalkBase (1000) + n, and the ranges sit above that. A group with many
// entries owns a range (base + n); a group with a few has fixed ids.
#pragma once

namespace ote {

constexpr int kTalkBase = 1000;      // + n: the n-th Talk entry (BattleScene::MenuId uses this value)
constexpr int kAbilityBase = 2000;   // + n: the n-th ability entry of the menu now open (U3)
constexpr int kStealBase = 2500;     // + n: the item in slot n of the foe, in a steal ability's "Steal which?" list (docs/SRPG.md "Steal"), n < 500
constexpr int kSupportBase = 3000;   // + n: the n-th "Support: <partner>" entry (U5)
constexpr int kStaffBase = 4000;     // + n: the staff in inventory slot n, in the Staff list (docs/SRPG.md section 8)
constexpr int kStaffItemBase = 4500; // + n: the item in slot n of the target, in a repair staff's "Repair which?" menu (stage 2)
constexpr int kStaffEnd = 5000;      // first id past the staff ranges
constexpr int kPlaceBase = 5000;     // + n: the n-th entry of Battle::placeOptions (Open / Visit / Destroy; docs/SRPG.md section 10), n < 100
constexpr int kPlaceKeyBase = 5100;  // + n: the key in inventory slot n, in the "Which key?" menu
constexpr int kPlaceBreakEntry = 5200;  // "Break door"
constexpr int kSrpgRangeEnd = 5300;  // first id past the ranges

enum SrpgMenuId : int {
  kRescue = 100, kDrop, kGive, kTake,  // carry (U4)
  kPairUp, kSwitch,                    // pair up (U4); Drop is shared with carry
  kPromote,                            // U2
  kInfo,                               // action menu "Info": the unit details (U1)
  kUnitEntry,                          // battle (End Turn) menu "Unit": the details of the unit under the cursor (U1)
  kStaffEntry                          // action menu "Staff": opens the list of the staves carried
};

// Which group an id of the action menu belongs to (the dispatcher of battle_srpg_menu.cpp sends it there).
enum class SrpgGroup { None, Talk, Ability, Support, Rescue, Promote, Info, Staff, Place };

inline SrpgGroup srpgGroupOf(int id) {
  if (id >= kTalkBase && id < kAbilityBase) return SrpgGroup::Talk;
  if (id >= kAbilityBase && id < kSupportBase) return SrpgGroup::Ability;
  if (id >= kSupportBase && id < kStaffBase) return SrpgGroup::Support;
  if (id >= kStaffBase && id < kStaffEnd) return SrpgGroup::Staff;
  if (id >= kPlaceBase && id < kSrpgRangeEnd) return SrpgGroup::Place;
  switch (id) {
    case kRescue: case kDrop: case kGive: case kTake: case kPairUp: case kSwitch: return SrpgGroup::Rescue;
    case kPromote: return SrpgGroup::Promote;
    case kInfo: return SrpgGroup::Info;
    case kStaffEntry: return SrpgGroup::Staff;
    default: return SrpgGroup::None;
  }
}

}  // namespace ote
