// The player's side of the action-point turn system (docs/SRPG.md "Action points", "Player"): the price on the Attack entry, the Move entry
// that lets a unit walk on after its first move, attacks the pool cannot pay left out of the target list, and the AP price of the square
// under the cursor in the hint line. The rules are all in the core (Battle::ap*); this file only asks them.
#include <algorithm>

#include "battle/battle_scene.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {

void BattleScene::apKeepAffordable(std::vector<AttackOption>& opts) const {
  if (!battle_.apGame() || sel_ < 0) return;
  const Unit& u = battle_.units[(size_t)sel_];
  const int pool = battle_.apPool(u.side);
  const bool none = battle_.apAttacksLeft(u) <= 0;
  opts.erase(std::remove_if(opts.begin(), opts.end(), [&](const AttackOption& o) { return none || battle_.apAttackCost(sel_, o.weapon) > pool; }),
             opts.end());
}

void BattleScene::apActionEntries(std::vector<MenuItem>& items, const std::vector<AttackOption>& opts) const {
  if (!battle_.apGame() || sel_ < 0) return;
  const Unit& u = battle_.units[(size_t)sel_];
  size_t after = 0;  // the Move entry goes right behind Attack
  for (size_t i = 0; i < items.size(); ++i) {
    if (items[i].id != kAttack) continue;
    std::vector<AttackOption> affordable = opts;
    apKeepAffordable(affordable);
    if (affordable.empty()) {  // something to hit, but no attack left or no AP for it: greyed, with the reason
      items[i].enabled = false;
      items[i].extra = battle_.apAttacksLeft(u) <= 0 ? "used" : "no AP";
    } else {
      int cheapest = battle_.apAttackCost(sel_, affordable[0].weapon);
      for (const AttackOption& o : affordable) cheapest = std::min(cheapest, battle_.apAttackCost(sel_, o.weapon));
      items[i].extra = intStr(cheapest) + " AP";
    }
    after = i + 1;
  }
  if (battle_.apMayMove(u)) items.insert(items.begin() + (long)after, MenuItem{"Move", "", true, kMoveOnEntry});
}

void BattleScene::beginMoveOn() {
  menu_.close();
  movingOn_ = true;
  mode_ = Mode::Selected;
  cursor_ = battle_.units[(size_t)sel_].cell;
  battle_.reachable(sel_, reach_);
  computeOverlay();
}

std::string BattleScene::apSelectedHint() const {
  const Unit& u = battle_.units[(size_t)sel_];
  std::string text;
  if (cursor_ != u.cell && reach_.canStop(cursor_)) {
    const int cost = battle_.apMoveCost(u, reach_.cost[(size_t)cursor_]);
    text = "{ok}:move " + intStr(cost) + " AP  {dpad}:cursor  {cancel}:cancel";  // (the price first: a big UI scale cuts the end of the line)
  } else {
    text = "{dpad}:cursor  {ok}:move here (own cell = stay)  {cancel}:cancel";
  }
  static std::string last;  // once per change, for the player tests
  if (text != last) {
    last = text;
    OTE_INFO("move hint: %s (AP %d)", text.c_str(), battle_.apPool(u.side));
  }
  return text;
}

}  // namespace ote
