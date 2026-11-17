// The strategy-RPG entry groups of the battle menus (docs/SRPG.md "Player UI"). This file only decides the ORDER of
// the groups and sends a chosen id to the group that owns it; every group lives in its own file
// (battle_menu_supports / _abilities / _rescue / _promotion / _info.cpp), empty until its step (U1..U5) fills it.
//
// Action menu order from the contract: Attack, Capture, Talk, Support, abilities, Rescue.., Promote, Items, Trade,
// Storage, Shop, Supply, Info, Wait. battle_menus.cpp calls addSrpgEntries right after Talk and addSrpgInfoEntry
// right before Wait.
#include "battle/battle_scene.h"

namespace ote {

void BattleScene::addSrpgEntries(std::vector<MenuItem>& items) {
  addSupportEntries(items);
  addAbilityEntries(items);
  addStaffEntries(items);
  addPlaceEntries(items);  // doors, chests and visiting (battle_menu_places.cpp)
  addRescueEntries(items);
  addPromotionEntries(items);
}

void BattleScene::addSrpgInfoEntry(std::vector<MenuItem>& items) { addInfoEntry(items); }

void BattleScene::addSrpgEndEntries(std::vector<MenuItem>& items) { addUnitEntry(items); }

bool BattleScene::srpgMenuChosen(int id) {
  switch (srpgGroupOf(id)) {
    case SrpgGroup::Support: return supportChosen(id);
    case SrpgGroup::Ability: return abilityChosen(id);
    case SrpgGroup::Staff: return staffChosen(id);
    case SrpgGroup::Place: return placeChosen(id);
    case SrpgGroup::Rescue: return rescueChosen(id);
    case SrpgGroup::Promote: return promotionChosen(id);
    case SrpgGroup::Info: return infoChosen(id);
    default: return false;  // None and Talk are not ours
  }
}

bool BattleScene::srpgEndMenuChosen(int id) { return id == kUnitEntry && unitEntryChosen(id); }

}  // namespace ote
