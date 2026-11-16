// Popup menus of the battle scene: action, build and end-turn menus.
#include "battle/battle_scene.h"
#include "battle/economy_text.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {

// Puts the menu beside the unit, on the first side where it does not cover
// another unit (an adjacent enemy is exactly what the player wants to see).
void BattleScene::anchorActionMenu() {
  const Unit& u = battle_.units[sel_];
  int mw = menu_.w(), mh = menu_.h();
  int sx, sy, bw, bh;
  cellScreenBox(u.cell, sx, sy, bw, bh);
  if (proj_.raised()) {  // keep clear of the unit standing above the square's middle
    sy -= proj_.unitW();
    bh += proj_.unitW();
  }
  const int cand[4][2] = {{sx + bw + 2, sy}, {sx - mw - 2, sy}, {sx, sy + bh + 2}, {sx, sy - mh - 2}};
  // Pass 0 avoids every unit; pass 1 only avoids enemies (surrounded by allies).
  for (int pass = 0; pass < 2; ++pass)
    for (int i = 0; i < 4; ++i) {
      menu_.place(cand[i][0], cand[i][1], layout_.vw, layout_.vh);
      bool clear = true;
      for (const Unit& o : battle_.units) {
        if (!o.alive || o.id == u.id || (pass == 1 && !battle_.isEnemy(u, o))) continue;
        if ((size_t)o.id < view_.units.size() && !view_.units[(size_t)o.id].visible) continue;  // fog: an unseen unit must not steer the menu
        int ox, oy, ow, oh;
        cellScreenBox(o.cell, ox, oy, ow, oh);
        if (ox < menu_.x() + mw && ox + ow > menu_.x() && oy < menu_.y() + mh && oy + oh > menu_.y()) clear = false;
      }
      if (clear) return;
    }
  menu_.place(cand[0][0], cand[0][1], layout_.vw, layout_.vh);
}

void BattleScene::openActionMenu() {
  mode_ = Mode::ActionMenu;
  overlay_ = Overlay();
  staffList_ = false;
  staffFcOn_ = false;
  const Unit& u = battle_.units[sel_];
  std::vector<AttackOption> opts;
  battle_.attackOptions(sel_, u.cell, opts);
  std::vector<MenuItem> items;
  if (u.acted) {  // "move again": its action is used up, only Info and Wait are left (docs/SRPG.md "Player UI")
    addSrpgInfoEntry(items);
    items.push_back({"Wait", "", true, kWait});
    menu_.open(host_.renderer(), "", items, 0, 0, layout_.vw, layout_.vh);
    anchorActionMenu();
    return;
  }
  if (!opts.empty()) items.push_back({"Attack", "", true, kAttack});
  if (battle_.canCaptureAt(sel_, u.cell)) items.push_back({"Capture", "", true, kCapture});
  addTalkEntries(items);  // story: one entry per partner / event (docs/EVENTS.md)
  addSrpgEntries(items);  // strategy-RPG groups (battle_srpg_menu.cpp); none in a game without those features
  // Item entries (docs/ITEMS.md "Player"); none of them exist in a game without items.
  if (!u.items.empty()) items.push_back({"Items", "", true, kItems});
  std::vector<int> partners, shop;
  battle_.tradePartners(sel_, partners);
  bool anyItems = !u.items.empty();
  for (int p : partners) anyItems |= !battle_.units[(size_t)p].items.empty();
  if (!partners.empty() && anyItems) items.push_back({"Trade", "", true, kTrade});
  if (battle_.storageOpen(u)) items.push_back({"Storage", "", true, kStorage});
  battle_.shopItems(sel_, shop);
  if (!shop.empty()) items.push_back({"Shop", "", true, kShop});
  if (battle_.canSupply(sel_)) items.push_back({EconomyWords::kSupply, "", true, kSupply});
  addSrpgInfoEntry(items);  // Info (U1)
  items.push_back({"Wait", "", true, kWait});
  menu_.open(host_.renderer(), "", items, 0, 0, layout_.vw, layout_.vh);
  anchorActionMenu();
}

void BattleScene::openBuildMenu(Cell cell) {
  mode_ = Mode::BuildMenu;
  buildCell_ = cell;
  std::vector<int> types;
  battle_.buildOptions(cell, types, true);  // unit types the side has not researched stay in, greyed
  const Database& db = *battle_.db;
  std::vector<MenuItem> items;
  std::string log;
  for (int t : types) {
    MenuItem it;
    it.label = db.unitTypes[t].name;
    for (const Amount& a : db.unitTypes[t].cost)
      it.extra += (it.extra.empty() ? "" : " ") + intStr(a.amount) + " " + db.resources[a.resource].name;
    it.enabled = battle_.canAfford(battle_.activeSide, t);
    if (!battle_.unitUnlocked(battle_.activeSide, t)) {  // the reason ("needs Armor") takes the price's place
      std::string why;
      battle_.canBuild(cell, t, &why);
      it.extra = why;
      it.enabled = false;
    }
    it.id = t;
    items.push_back(it);
    log += (log.empty() ? "" : "; ") + it.label + " (" + it.extra + ")" + (it.enabled ? "" : " [greyed]");
  }
  OTE_INFO("build menu: %s", log.c_str());
  int sx, sy, bw, bh;
  cellScreenBox(cell, sx, sy, bw, bh);
  menu_.open(host_.renderer(), "Build", items, sx + bw + 2, sy, layout_.vw, layout_.vh);
  menu_.reanchor(sx + bw + 2, sy, sx - 2, layout_.vw, layout_.vh);
}

void BattleScene::openEndMenu() {
  mode_ = Mode::EndMenu;
  std::vector<MenuItem> items = {{"End Turn", "", true, kEndTurn}};
  if (!battle_.objectiveText().empty()) items.push_back({"Objective", "", true, kObjective});  // only when the map has one
  addSrpgEndEntries(items);  // Unit (U1)
  // Research (docs/ECONOMY.md "Player"): whenever the game has technologies. With "needsStructure" and no research
  // structure it still opens; the panel greys its rows with the reason.
  if (!session_.db.research.empty() && humanTurn()) items.push_back({EconomyWords::kResearch, "", true, kResearchEntry});
  if (manualSaves()) {  // game setting "any": the player may save and load at will
    items.push_back({"Save", "", true, kSave});
    items.push_back({"Load", "", true, kLoad});
  }
  // With a Continue save the battle can be picked up later; without one, leaving ends it.
  items.push_back({continueOn() ? "Leave battle" : "Abandon battle", "", true, kLeave});
  items.push_back({"Cancel", "", true, 0});
  std::string log;
  for (const MenuItem& it : items) log += (log.empty() ? "" : ", ") + it.label;
  OTE_INFO("battle menu: %s", log.c_str());
  menu_.open(host_.renderer(), "", items, layout_.vw / 2 - 40, layout_.vh / 2 - 20, layout_.vw, layout_.vh);
  menu_.place(layout_.vw / 2 - menu_.w() / 2, layout_.vh / 2 - menu_.h() / 2, layout_.vw, layout_.vh);
}

void BattleScene::menuInput(const InputEvent& e) {
  Menu::Result res = menu_.onInput(e);
  if (res == Menu::None) return;
  int id = menu_.chosenId();
  if (mode_ == Mode::ActionMenu) {
    if (res == Menu::Cancelled && staffList_) {  // the Staff list: back to the action menu, nothing undone
      openActionMenu();
      return;
    }
    if (res == Menu::Cancelled) {
      // An item action cannot be undone, and the walk back would undo it with the move.
      if (itemsUsed_) say("Cannot undo after using items");
      else cancelAction();
      return;
    }
    if (id == kAttack) beginTargeting(-1);
    else if (srpgGroupOf(id) == SrpgGroup::Talk) talkChosen(id - kTalkBase);
    else if (srpgGroupOf(id) != SrpgGroup::None && srpgMenuChosen(id)) {}  // a strategy-RPG group took it
    else if (id == kItems) openItems();
    else if (id == kTrade) openTrade();
    else if (id == kStorage) openStorage();
    else if (id == kShop) openShop();
    else if (id == kSupply) { if (submit(Action::supply(sel_))) finishOrder(); }
    else if (id == kCapture) { if (submit(Action::capture(sel_))) finishOrder(); }
    else if (id == kWait) { if (submit(Action::wait(sel_))) finishOrder(); }
  } else if (mode_ == Mode::StructureMenu || mode_ == Mode::UpgradeMenu) {
    if (res == Menu::Cancelled) {
      if (mode_ == Mode::UpgradeMenu) backToStructure();
      else { menu_.close(); mode_ = Mode::Idle; }
    } else economyMenuChosen(id);
  } else if (mode_ == Mode::BuildMenu) {
    menu_.close();
    mode_ = Mode::Idle;
    if (res == Menu::Chosen) submit(Action::build(buildCell_, id));
    else if (structureMenuBack_) backToStructure();
  } else if (mode_ == Mode::EndMenu) {
    menu_.close();
    mode_ = Mode::Idle;
    if (res == Menu::Chosen && id == kEndTurn) submit(Action::endTurn());
    else if (res == Menu::Chosen && id == kObjective) openObjective();
    else if (res == Menu::Chosen && id == kResearchEntry) openResearchPanel();
    else if (res == Menu::Chosen && srpgEndMenuChosen(id)) {}
    else if (res == Menu::Chosen && id == kSave) openSlotList(SlotPanel::Save);
    else if (res == Menu::Chosen && id == kLoad) openSlotList(SlotPanel::Load);
    else if (res == Menu::Chosen && id == kLeave) {
      if (continueOn()) writeContinue();  // keep the very latest position, including a unit that has only moved
      host_.showGame(host_.currentGameId());  // replaces this scene: do nothing after this line
      return;
    }
  }
}

}  // namespace ote
