// Action menu: "Support: <partner>" entries, ids kSupportBase + n (docs/SRPG.md "Player UI", step U5).
// One entry per partner with a ready conversation next to the unit. The partner is fixed by the entry, so there is no
// target to pick; choosing plays the conversation in the battle's dialogue box. The action is free: the unit can
// still act afterwards (the menu is rebuilt, like after using an item) and the move can no longer be taken back.
#include "battle/battle_scene.h"
#include "battle/support_text.h"
#include "core/base/log.h"

namespace ote {

void BattleScene::addSupportEntries(std::vector<MenuItem>& items) {
  std::vector<int> partners;
  battle_.supportOptions(sel_, battle_.units[(size_t)sel_].cell, partners);  // empty in a game without supports
  std::string log;
  for (size_t i = 0; i < partners.size(); ++i) {
    items.push_back({supportMenuLabel(battle_.units[(size_t)partners[i]].name), "", true, kSupportBase + (int)i});
    log += (log.empty() ? "" : ", ") + items.back().label;
  }
  // A game with pairs logs the entries every time the menu opens (scripts read this); a game without says nothing.
  if (!battle_.db->supports.pairs.empty()) OTE_INFO("support entries: %s", log.empty() ? "none" : log.c_str());
}

bool BattleScene::supportChosen(int id) {
  // The partners are asked for again: the same list in the same order as when the menu was built, because nothing
  // happens between building the menu and choosing from it.
  std::vector<int> partners;
  battle_.supportOptions(sel_, battle_.units[(size_t)sel_].cell, partners);
  const int n = id - kSupportBase;
  if (n < 0 || (size_t)n >= partners.size()) return true;  // stale entry: ours, nothing to do
  OTE_INFO("support: %s talks with %s", battle_.units[(size_t)sel_].name.c_str(),
           battle_.units[(size_t)partners[(size_t)n]].name.c_str());
  if (!submit(Action::support(sel_, partners[(size_t)n]))) return true;
  const Unit& u = battle_.units[(size_t)sel_];  // after submit: a story event may have added units
  if (battle_.over || !u.alive || u.done) finishOrder();
  else itemActionDone();
  return true;
}

}  // namespace ote
