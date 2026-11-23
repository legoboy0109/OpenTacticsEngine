// The item screens of the battle scene (docs/ITEMS.md "Player"): Items (with
// Use / Equip / Store / Discard), Trade, Storage and Shop. They are all
// reached from the action menu and, except for Use, hand control back to it:
// the unit has not finished its turn. All changes go through submit(), so the
// view animates the events and the Continue save is written.
#include "battle/battle_scene.h"
#include "battle/item_text.h"
#include "battle/storage_stack.h"
#include "battle/rank_text.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {
namespace {

// `holder` (optional) is the unit that carries it: a weapon its rank does not allow is greyed with the reason.
PanelRow stackRow(const Battle& b, const ItemStack& s, bool equipped, const Unit* holder = nullptr) {
  PanelRow row;
  row.label = b.itemOf(s).name;
  row.pic = itemicon::ofItem(*b.db, s.item);
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

void BattleScene::openTradeWith(int partner, int col, int row) {
  mode_ = Mode::Items;
  itemScreen_ = ItemScreen::Trade;
  partner_ = partner;
  menu_.close();
  const Unit& u = battle_.units[sel_];
  const Unit& p = battle_.units[(size_t)partner];
  int size = session_.db.rules.items.inventorySize;
  tradePanel_.open(u.name, inventoryRows(battle_, u, size), p.name, inventoryRows(battle_, p, size), col, row);
  // The screen tests read these: both packs in slot order, and where each slot is on screen (to tap it).
  std::string packs, spots;
  for (const Unit* w : {&u, &p}) {
    packs += (packs.empty() ? "" : " | ") + w->name + " [";
    for (size_t i = 0; i < w->items.size(); ++i) packs += (i ? ", " : "") + battle_.itemOf(w->items[i]).name;
    packs += "]";
  }
  tradePanel_.setArea(0, 0, layout_.vw, layout_.vh);
  for (int c = 0; c < 2; ++c)
    for (int i = 0; i < size; ++i) {
      int x, y;
      tradePanel_.rowCentre(host_.renderer(), c, i, x, y);
      spots += " " + intStr(c) + "." + intStr(i) + "@" + intStr(x) + "," + intStr(y);
    }
  OTE_INFO("trade: %s", packs.c_str());
  OTE_INFO("trade rows:%s", spots.c_str());
}

void BattleScene::openStorage() {
  mode_ = Mode::Items;
  itemScreen_ = ItemScreen::Storage;
  menu_.close();
  const std::vector<ItemStack>& store = battle_.sides[(size_t)battle_.units[sel_].side].storage;
  std::vector<PanelRow> rows;
  for (const StorageRow& g : stackStorage(store)) {  // identical copies are one row with a count
    ItemStack s;
    s.item = g.item;
    s.uses = g.uses;
    s.drop = g.drop;
    PanelRow row = stackRow(battle_, s, false);
    row.count = g.count;
    rows.push_back(row);
  }
  itemPanel_.open("Storage", "", rows, "The storage is empty.");
  std::string log;  // the screen tests read this: one entry per row, "Potion 3/3 x3"
  for (const PanelRow& r : rows)
    log += (log.empty() ? "" : ", ") + r.label + (r.extra.empty() ? "" : " " + r.extra) + (r.count > 1 ? " x" + intStr(r.count) : "");
  OTE_INFO("storage: %d rows [%s]", (int)rows.size(), log.c_str());
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

// Confirm on the trade screen. First an item is picked up (framed); then a slot of either unit: an item there swaps places with it,
// a free slot takes the item. Each change is made for good at once (the core applies it), so Back afterwards keeps what was done and the
// screen stays open for the next one. Confirm on the picked-up item puts it back; Back while holding only puts it back.
void BattleScene::tradePicked() {
  const Unit& u = battle_.units[sel_];
  const Unit& p = battle_.units[(size_t)partner_];
  const int col = tradePanel_.col(), row = tradePanel_.row();
  const Unit& here = col == 0 ? u : p;
  const bool item = row < (int)here.items.size();
  if (!tradePanel_.holding()) {
    if (!item) { say("Nothing to pick up"); return; }
    tradePanel_.hold(col, row);
    OTE_INFO("trade: picked up %s from %s", battle_.itemOf(here.items[(size_t)row]).name.c_str(), here.name.c_str());
    return;
  }
  const int hc = tradePanel_.heldCol(), hr = tradePanel_.heldRow();
  if (hc == col && hr == row) { tradePanel_.release(); return; }
  Action act;
  if (!item) {
    if (hc == col) return;  // a free slot of the same unit: nothing to do
    if (!battle_.hasRoom(here)) { say(here.name + " cannot carry more"); return; }
    act = hc == 0 ? Action::giveItem(sel_, hr, partner_) : Action::takeItem(sel_, partner_, hr);
  } else if (hc == col) {
    if (col == 1) { say("Only " + u.name + "'s own items can be put in order"); return; }  // the other unit is not the one acting
    act = Action::swapItems(sel_, hr, sel_, row);
  } else {
    act = hc == 0 ? Action::swapItems(sel_, hr, partner_, row) : Action::swapItems(sel_, row, partner_, hr);
  }
  const std::string what = (!item ? "move " : "swap ") + battle_.itemOf((hc == 0 ? u : p).items[(size_t)hr]).name;
  if (!submit(act)) return;
  OTE_INFO("trade: %s (%s with %s)", what.c_str(), u.name.c_str(), p.name.c_str());
  itemsUsed_ = true;
  hasUndo_ = false;
  openTradeWith(partner_, col, row);  // the new packs, cursor where it was, nothing picked up
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
      if (res == TradePanel::Back) {
        if (tradePanel_.holding()) tradePanel_.release();  // Back puts the picked-up item down first
        else { closeItems(); openActionMenu(); }
      } else if (res == TradePanel::Picked) tradePicked();
      break;
    }
    case ItemScreen::Storage: {
      ItemPanel::Result res = itemPanel_.onInput(e, r);
      if (res == ItemPanel::Back) { closeItems(); openActionMenu(); }
      else if (res == ItemPanel::Picked) {
        if (!battle_.hasRoom(u)) say(u.name + " cannot carry more");
        else {  // one copy: the first of the row's identical ones
          const std::vector<StorageRow> rows = stackStorage(battle_.sides[(size_t)u.side].storage);
          if (itemPanel_.sel() >= 0 && itemPanel_.sel() < (int)rows.size()) submitItem(Action::fetchItem(sel_, rows[(size_t)itemPanel_.sel()].index));
        }
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

// What Confirm does on the trade screen right now: pick up, or put down (swap with an item, move into a free slot).
std::string BattleScene::tradeHint() const {
  if (!tradePanel_.holding()) return "{leftright}:unit  {updown}  {ok}:pick up  {cancel}:back";
  const Unit& u = battle_.units[sel_];
  const Unit& p = battle_.units[(size_t)partner_];
  const Unit& here = tradePanel_.col() == 0 ? u : p;
  const bool item = tradePanel_.row() < (int)here.items.size();
  const bool same = tradePanel_.col() == tradePanel_.heldCol() && tradePanel_.row() == tradePanel_.heldRow();
  if (same) return "{leftright}:unit  {updown}  {ok}:put back  {cancel}:put back";
  return item ? "{leftright}:unit  {updown}  {ok}:swap  {cancel}:put back" : "{leftright}:unit  {updown}  {ok}:move here  {cancel}:put back";
}

std::string BattleScene::itemsHint() const {
  switch (itemScreen_) {
    case ItemScreen::List: return "{udok}:choose  {cancel}:back";
    case ItemScreen::Choice:
    case ItemScreen::Target:
    case ItemScreen::Partner: return "{udok}:choose  {cancel}:back";
    case ItemScreen::Discard: return "{lrok}:answer  {cancel}:keep it";
    case ItemScreen::Trade: return tradeHint();
    case ItemScreen::Storage: return "{udok}:take  {cancel}:back";
    case ItemScreen::Shop: return "{udok}:buy  {cancel}:back";
  }
  return "";
}

}  // namespace ote
