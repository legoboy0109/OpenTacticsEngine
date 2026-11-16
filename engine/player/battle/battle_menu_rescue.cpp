// Action menu: Rescue / Drop / Give / Take (carry) and Pair up / Switch / Drop (pair up), fixed ids kRescue..kSwitch
// (docs/SRPG.md "Player UI", step U4). What is offered is decided by rescue_menu.cpp (no SDL, unit tested); this file
// only turns it into menu items and runs the picks.
//
// Which actions end the turn follows the core: Rescue, Pair up and Drop end it (the order is over, back to the
// map); Give, Take and Switch are free (the action menu comes back, the move can no longer be undone, like an item).
#include "battle/battle_scene.h"
#include "battle/rescue_menu.h"
#include "core/base/log.h"

namespace ote {

void BattleScene::addRescueEntries(std::vector<MenuItem>& items) {
  std::string log;  // the script tests read this ("the player logs each opened menu's entries")
  for (const RescueEntry& e : rescueEntries(battle_, sel_)) {
    items.push_back({e.label, e.extra, e.enabled, e.id});
    log += (log.empty() ? "" : ", ") + e.label + (e.enabled ? "" : " (" + e.extra + ")");
  }
  if (!log.empty()) OTE_INFO("rescue entries of %s: %s", battle_.units[(size_t)sel_].name.c_str(), log.c_str());
}

bool BattleScene::rescueChosen(int id) {
  if (sel_ < 0) return false;
  const int unit = sel_;
  // After an action the picks have cleared the mode; a refused action (the UI offered what the rules forbid; logged
  // by submit) must not leave the player with no menu.
  auto done = [this, unit](bool ok, bool endsTurn) {
    if (!ok) openActionMenu();
    else if (endsTurn || battle_.over) finishOrder();
    else {
      cursor_ = battle_.units[(size_t)unit].cell;  // back on the acting unit (the pick had moved it to the target)
      itemActionDone();
    }
  };
  switch (id) {
    case kRescue: case kPairUp: case kGive: case kTake: {
      std::vector<int> targets = rescuePickUnits(battle_, unit, id);
      if (targets.empty()) return true;  // nothing legal: the menu stays (it was not offered, but be safe)
      const bool endsTurn = id == kRescue || id == kPairUp;
      beginPickUnit(std::move(targets), [this, id, unit, done, endsTurn](int target) {
        Action a = id == kRescue ? Action::rescue(unit, target) : id == kPairUp ? Action::pairUp(unit, target)
                 : id == kGive ? Action::giveUnit(unit, target) : Action::takeUnit(unit, target);
        done(submit(a), endsTurn);
      });
      return true;
    }
    case kDrop: {
      std::vector<Cell> cells = rescuePickCells(battle_, unit);
      if (cells.empty()) return true;  // greyed in the menu; nothing to do
      beginPickCell(std::move(cells), [this, unit, done](Cell cell) { done(submit(Action::drop(unit, cell)), true); });
      return true;
    }
    case kSwitch: {
      const Cell at = battle_.units[(size_t)unit].cell;
      if (!submit(Action::switchPair(unit))) { openActionMenu(); return true; }
      // The two exchanged places: the one now in front has the turn flags and goes on from the action menu.
      const int front = battle_.unitAt(at);
      if (front >= 0 && battle_.canAct(front) && !battle_.units[(size_t)front].done) {
        sel_ = front;
        cursor_ = at;
        itemActionDone();
      } else {
        finishOrder();
      }
      return true;
    }
    default: return false;
  }
}

}  // namespace ote
