// The item screens of the battle scene (docs/ITEMS.md "Player"): Items (with
// Use / Equip / Store / Discard), Trade, Storage and Shop. They are all
// reached from the action menu and, except for Use, hand control back to it:
// the unit has not finished its turn. All changes go through submit(), so the
// view animates the events and the Continue save is written.
#include "battle/battle_scene.h"
#include "battle/item_text.h"
#include "battle/rank_text.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {
namespace {

// `holder` (optional) is the unit that carries it: a weapon its rank does not allow is greyed with the reason.
PanelRow stackRow(const Battle& b, const ItemStack& s, bool equipped, const Unit* holder = nullptr) {
  PanelRow row;
  row.label = b.itemOf(s).name;
  row.extra = usesText(b, s);
  row.detail = itemDetail(b, s);
  row.mark = equipped;
  const std::string blocked = holder ? weaponBlockReason(b, *holder, s) : "";
  if (!blocked.empty()) {  // docs/SRPG.md section 7: carried, traded and stored, but not wielded
    row.dim = true;
    row.detail = blocked;
  }
  return row;
}

// A unit's inventory as rows; `padTo` > 0 adds free slots ("-") up to that many rows.
std::vector<PanelRow> inventoryRows(const Battle& b, const Unit& u, int padTo) {
  std::vector<PanelRow> rows;
  int eq = b.db->rules.items.carriedWeapons ? b.equippedSlot(u) : -1;
  for (size_t i = 0; i < u.items.size(); ++i) rows.push_back(stackRow(b, u.items[i], (int)i == eq, &u));
  while ((int)rows.size() < padTo) {
    PanelRow free;
    free.label = "-";
    free.blank = true;
    rows.push_back(free);
  }
  return rows;
}

}  // namespace

void BattleScene::closeItems() {
  itemPanel_.close();
  tradePanel_.close();
  confirm_.close();
  menu_.close();
}

// Something that does not end the turn happened. Entries of the action menu may have changed
// (an item gone, a new weapon), so it is rebuilt; the move cannot be taken back any more.
void BattleScene::itemActionDone() {
  itemsUsed_ = true;
  hasUndo_ = false;
  closeItems();
  openActionMenu();
}

bool BattleScene::submitItem(const Action& a) {
  if (!submit(a)) return false;
  itemActionDone();
  return true;
}

// ---- opening the screens

void BattleScene::openItems(int slot) {
  mode_ = Mode::Items;
  itemScreen_ = ItemScreen::List;
  menu_.close();
  const Unit& u = battle_.units[sel_];
  std::string title = u.name + " - Items " + intStr((int)u.items.size()) + "/" + intStr(session_.db.rules.items.inventorySize);
  const std::vector<PanelRow> rows = inventoryRows(battle_, u, 0);
  itemPanel_.open(title, "", rows, "No items.", slot);
  std::string log;  // the screen tests read this; a weapon the rank does not allow carries its reason
  for (const PanelRow& r : rows) log += (log.empty() ? "" : ", ") + r.label + (r.dim ? " [" + r.detail + "]" : "");
  OTE_INFO("items: %s", log.c_str());
}

void BattleScene::openChoiceMenu() {
  const Unit& u = battle_.units[sel_];
  if (itemSlot_ < 0 || itemSlot_ >= (int)u.items.size()) { openItems(); return; }
  itemScreen_ = ItemScreen::Choice;
  const ItemDef& d = battle_.itemOf(u.items[(size_t)itemSlot_]);
  std::vector<int> targets;
  battle_.useTargets(sel_, itemSlot_, targets);
  std::vector<MenuItem> items;
  if (!targets.empty()) items.push_back({"Use", "", true, kUse});
  if (session_.db.rules.items.carriedWeapons && d.kind == ItemKind::Weapon &&
      session_.db.canWield(u.type, d.weapon, u.character) && itemSlot_ != battle_.equippedSlot(u)) {
    const std::string why = weaponBlockReason(battle_, u, u.items[(size_t)itemSlot_]);  // "Needs rank B": greyed, not chosen
    items.push_back({"Equip", why, why.empty(), kEquip});
  }
  if (battle_.storageOpen(u)) items.push_back({"Store", "", true, kStore});
  items.push_back({"Discard", "", true, kDiscard});
  Renderer& r = host_.renderer();
  menu_.open(r, d.name, items, 0, 0, layout_.vw, layout_.vh);
  int x, y, w, h;
  itemPanel_.selectedBox(r, x, y, w, h);
  menu_.place(x + w - menu_.w(), y + h, layout_.vw, layout_.vh);  // under the row, so the row stays readable
  std::string log;
  for (const MenuItem& m : items) log += (log.empty() ? "" : ", ") + m.label + (m.enabled ? "" : " [" + m.extra + "]");
  OTE_INFO("item menu: %s - %s", d.name.c_str(), log.c_str());
}

void BattleScene::openTrade() {
  mode_ = Mode::Items;
  std::vector<int> partners;
  battle_.tradePartners(sel_, partners);
  if (partners.empty()) { openActionMenu(); return; }
  if (partners.size() == 1) { openTradeWith(partners[0]); return; }
  itemScreen_ = ItemScreen::Partner;
  std::vector<MenuItem> items;
  for (int p : partners) items.push_back({battle_.units[(size_t)p].name, "", true, p});
  menu_.open(host_.renderer(), "Trade with", items, 0, 0, layout_.vw, layout_.vh);
  menu_.place(layout_.vw / 2 - menu_.w() / 2, layout_.vh / 2 - menu_.h() / 2, layout_.vw, layout_.vh);
}

void BattleScene::openTradeWith(int partner) {
  mode_ = Mode::Items;
  itemScreen_ = ItemScreen::Trade;
  partner_ = partner;
  menu_.close();
  const Unit& u = battle_.units[sel_];
  const Unit& p = battle_.units[(size_t)partner];
  int size = session_.db.rules.items.inventorySize;
  tradePanel_.open(u.name, inventoryRows(battle_, u, size), p.name, inventoryRows(battle_, p, size));
}

void BattleScene::openStorage() {
  mode_ = Mode::Items;
  itemScreen_ = ItemScreen::Storage;
  menu_.close();
  const std::vector<ItemStack>& store = battle_.sides[(size_t)battle_.units[sel_].side].storage;
  std::vector<PanelRow> rows;
  for (const ItemStack& s : store) rows.push_back(stackRow(battle_, s, false));
  itemPanel_.open("Storage", "", rows, "The storage is empty.");
}

void BattleScene::openShop() {
  mode_ = Mode::Items;
  itemScreen_ = ItemScreen::Shop;
  menu_.close();
  const Database& db = session_.db;
  const SideState& side = battle_.sides[(size_t)battle_.units[sel_].side];
  battle_.shopItems(sel_, shopList_);
  std::vector<PanelRow> rows;
  for (int i : shopList_) {
    const ItemDef& d = db.items[(size_t)i];
    ItemStack s;
    s.item = i;
    s.uses = d.uses;
    PanelRow row = stackRow(battle_, s, false);
    row.extra = priceText(db, d.price);  // what it costs, not its uses (a new item has all of them)
    row.dim = !battle_.canPay(battle_.units[sel_].side, d.price);
    rows.push_back(row);
  }
  std::string head;
  for (size_t i = 0; i < db.resources.size(); ++i)
    head += (head.empty() ? "" : "  ") + db.resources[i].name + " " + intStr(side.resources[i]);
  itemPanel_.open("Shop", head, rows, "Nothing for sale.");
}

// ---- input

void BattleScene::chooseUseTarget(int slot) {
  if (promotionItemUse(slot)) return;  // a promotion item: the question / promotion screen (battle_menu_promotion.cpp)
  std::vector<int> targets;
  battle_.useTargets(sel_, slot, targets);
  if (targets.empty()) return;
  if (targets.size() == 1) {
    closeItems();
    if (submit(Action::useItem(sel_, slot, targets[0]))) finishOrder();  // using an item ends the turn, like Wait
    else openItems(slot);
    return;
  }
  itemScreen_ = ItemScreen::Target;
  std::vector<MenuItem> items;
  for (int t : targets) {
    const Unit& tu = battle_.units[(size_t)t];
    items.push_back({tu.name, intStr(tu.hp) + "/" + intStr(battle_.maxHp(tu)), true, t});
  }
  Renderer& r = host_.renderer();
  menu_.open(r, "Use on", items, 0, 0, layout_.vw, layout_.vh);
  int x, y, w, h;
  itemPanel_.selectedBox(r, x, y, w, h);
  menu_.place(x + w - menu_.w(), y + h, layout_.vw, layout_.vh);  // under the row, so the row stays readable
}

void BattleScene::itemChoice(int id) {
  if (id == kUse) {
    chooseUseTarget(itemSlot_);
  } else if (id == kEquip) {
    submitItem(Action::equip(sel_, itemSlot_));
  } else if (id == kStore) {
    submitItem(Action::storeItem(sel_, itemSlot_));
  } else if (id == kDiscard) {
    const Unit& u = battle_.units[sel_];
    menu_.close();
    itemScreen_ = ItemScreen::Discard;
    confirm_.open("Discard " + battle_.itemOf(u.items[(size_t)itemSlot_]).name + "?", "Yes", "No");
  }
}

void BattleScene::tradePicked() {
  const Unit& u = battle_.units[sel_];
  const Unit& p = battle_.units[(size_t)partner_];
  int row = tradePanel_.row();
  if (tradePanel_.col() == 0) {
    if (row >= (int)u.items.size()) { say("Nothing to hand over"); return; }
    if (!battle_.hasRoom(p)) { say(p.name + " cannot carry more"); return; }
    submitItem(Action::giveItem(sel_, row, partner_));
  } else {
    if (row >= (int)p.items.size()) { say("Nothing to take"); return; }
    if (!battle_.hasRoom(u)) { say(u.name + " cannot carry more"); return; }
    submitItem(Action::takeItem(sel_, partner_, row));
  }
}

void BattleScene::itemsInput(const InputEvent& e) {
  Renderer& r = host_.renderer();
  itemPanel_.setArea(0, 0, layout_.vw, layout_.vh);
  tradePanel_.setArea(0, 0, layout_.vw, layout_.vh);
  const Unit& u = battle_.units[sel_];
  switch (itemScreen_) {
    case ItemScreen::List: {
      ItemPanel::Result res = itemPanel_.onInput(e, r);
      if (res == ItemPanel::Back) { closeItems(); openActionMenu(); }
      else if (res == ItemPanel::Picked) { itemSlot_ = itemPanel_.sel(); openChoiceMenu(); }
      break;
    }
    case ItemScreen::Choice: {
      Menu::Result res = menu_.onInput(e);
      if (res == Menu::Cancelled) { menu_.close(); openItems(itemSlot_); }
      else if (res == Menu::Chosen) itemChoice(menu_.chosenId());
      break;
    }
    case ItemScreen::Target: {
      Menu::Result res = menu_.onInput(e);
      if (res == Menu::Cancelled) openChoiceMenu();
      else if (res == Menu::Chosen) {
        int target = menu_.chosenId();
        closeItems();
        if (submit(Action::useItem(sel_, itemSlot_, target))) finishOrder();
        else openItems(itemSlot_);
      }
      break;
    }
    case ItemScreen::Discard: {
      ConfirmBox::Result res = confirm_.onInput(e, r);
      if (res == ConfirmBox::None) break;
      confirm_.close();
      if (res == ConfirmBox::Yes) submitItem(Action::discardItem(sel_, itemSlot_));
      else openChoiceMenu();
      break;
    }
    case ItemScreen::Partner: {
      Menu::Result res = menu_.onInput(e);
      if (res == Menu::Cancelled) { closeItems(); openActionMenu(); }
      else if (res == Menu::Chosen) openTradeWith(menu_.chosenId());
      break;
    }
    case ItemScreen::Trade: {
      TradePanel::Result res = tradePanel_.onInput(e, r);
      if (res == TradePanel::Back) { closeItems(); openActionMenu(); }
      else if (res == TradePanel::Picked) tradePicked();
      break;
    }
    case ItemScreen::Storage: {
      ItemPanel::Result res = itemPanel_.onInput(e, r);
      if (res == ItemPanel::Back) { closeItems(); openActionMenu(); }
      else if (res == ItemPanel::Picked) {
        if (!battle_.hasRoom(u)) say(u.name + " cannot carry more");
        else submitItem(Action::fetchItem(sel_, itemPanel_.sel()));
      }
      break;
    }
    case ItemScreen::Shop: {
      ItemPanel::Result res = itemPanel_.onInput(e, r);
      if (res == ItemPanel::Back) { closeItems(); openActionMenu(); }
      else if (res == ItemPanel::Picked) {
        int item = shopList_[(size_t)itemPanel_.sel()];
        const ItemDef& d = session_.db.items[(size_t)item];
        if (!battle_.canPay(u.side, d.price)) say("Not enough to pay for " + d.name);
        else if (!battle_.hasRoom(u) && session_.db.rules.items.storage == ItemStorage::Off) say(u.name + " cannot carry more");
        else submitItem(Action::buyItem(sel_, item));  // a full unit's purchase goes to storage (the view says so)
      }
      break;
    }
  }
}

void BattleScene::drawItems(Renderer& r) {
  itemPanel_.setArea(0, 0, layout_.vw, layout_.vh);
  tradePanel_.setArea(0, 0, layout_.vw, layout_.vh);
  itemPanel_.draw(r);
  tradePanel_.draw(r);
  menu_.draw(r);
  confirm_.draw(r);
}

std::string BattleScene::itemsHint() const {
  switch (itemScreen_) {
    case ItemScreen::List: return "Up/Down + Z:choose  X:back";
    case ItemScreen::Choice:
    case ItemScreen::Target:
    case ItemScreen::Partner: return "Up/Down + Z:choose  X:back";
    case ItemScreen::Discard: return "Left/Right + Z:answer  X:keep it";
    case ItemScreen::Trade: return "Left/Right:unit  Up/Down  Z:hand over  X:back";
    case ItemScreen::Storage: return "Up/Down + Z:take  X:back";
    case ItemScreen::Shop: return "Up/Down + Z:buy  X:back";
  }
  return "";
}

}  // namespace ote
