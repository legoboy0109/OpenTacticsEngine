// The item parts of the preparation screen (docs/EVENTS.md "Preparation"): whose items, who to trade with (another
// character or the shared storage), the trade panel, and the preparation shop. They reuse the panels of the battle's own
// item screens (ItemPanel, TradePanel). Everything works on the playthrough's roster and what the side carries
// (core/campaign/prep.h), then the Battle shown is rebuilt so the unit details agree.
#include "battle/battle_prep_ui.h"
#include "battle/item_text.h"
#include "battle/prep_text.h"
#include "core/base/log.h"
#include "sound/cue_sink.h"
#include "ui/text.h"

namespace ote {
namespace {

ItemStack stackOf(const ItemGrant& g) {
  ItemStack s;
  s.item = g.item;
  s.uses = g.uses;
  s.drop = g.drop;
  return s;
}

// One inventory as rows; `padTo` > 0 adds free slots ("-") up to that many rows.
std::vector<PanelRow> grantRows(const Battle& b, const std::vector<ItemGrant>& items, int padTo) {
  std::vector<PanelRow> rows;
  int equipped = -1;  // with carried weapons the first weapon is the one in hand
  if (b.db->rules.items.carriedWeapons)
    for (size_t i = 0; i < items.size() && equipped < 0; ++i)
      if (b.db->items[(size_t)items[i].item].kind == ItemKind::Weapon) equipped = (int)i;
  for (size_t i = 0; i < items.size(); ++i) {
    const ItemStack s = stackOf(items[i]);
    PanelRow row;
    row.label = b.itemOf(s).name;
    row.extra = usesText(b, s);
    row.detail = itemDetail(b, s);
    row.mark = (int)i == equipped;
    rows.push_back(row);
  }
  while ((int)rows.size() < padTo) {
    PanelRow free;
    free.label = "-";
    free.blank = true;
    rows.push_back(free);
  }
  return rows;
}

const RosterUnit* member(const Playthrough& pt, const std::string& id) {
  const RosterUnit* r = pt.findUnit(id);
  return r && !r->gone ? r : nullptr;
}

std::string countText(const Database& db, const RosterUnit& r) {
  return intStr((int)r.items.size()) + "/" + intStr(db.rules.items.inventorySize);
}

std::string fundsText(const Database& db, const std::vector<int>& have) {
  std::string head;
  for (size_t i = 0; i < db.resources.size() && i < have.size(); ++i)
    head += (head.empty() ? "" : "  ") + db.resources[i].name + " " + intStr(have[i]);
  return head;
}

}  // namespace

// ---- whose items

void BattleScene::prepItemsWhoOpen(int sel) {
  PrepUi& p = *prep_;
  p.screen = PrepUi::Screen::ItemsWho;
  menu_.close();
  tradePanel_.close();
  std::vector<PanelRow> rows;
  p.ids.clear();
  std::string log;
  for (const RosterUnit& r : p.pt->roster) {
    if (r.gone) continue;
    PanelRow row;
    row.label = r.name;
    row.extra = countText(session_.db, r);
    rows.push_back(row);
    p.ids.push_back(r.id);
    log += (log.empty() ? "" : ", ") + r.name + " [" + row.extra + "]";
  }
  itemPanel_.open(PrepWords::kWhoseItems, "", rows, PrepWords::kNobody, sel);
  OTE_INFO("prep items: %s", log.c_str());
}

// Who to trade with: every other character, then the storage (when the game has one).
void BattleScene::prepItemsWithOpen() {
  PrepUi& p = *prep_;
  p.screen = PrepUi::Screen::ItemsWith;
  std::vector<PanelRow> rows;
  std::vector<std::string> ids;
  for (const RosterUnit& r : p.pt->roster) {
    if (r.gone || r.id == p.who) continue;
    PanelRow row;
    row.label = r.name;
    row.extra = countText(session_.db, r);
    rows.push_back(row);
    ids.push_back(r.id);
  }
  if (session_.db.rules.items.storage != ItemStorage::Off) {
    PanelRow row;
    row.label = PrepWords::kStorage;
    row.extra = intStr((int)prepStorage(*p.pt, p.map).size());
    rows.push_back(row);
    ids.push_back("");
  }
  if (rows.empty()) {
    say(PrepWords::kNoOneToTrade);
    prepItemsWhoOpen();
    return;
  }
  p.ids = ids;
  const RosterUnit* a = member(*p.pt, p.who);
  itemPanel_.open(PrepWords::kTradeWith, a ? a->name : p.who, rows, "", 0);
}

void BattleScene::prepTradeOpen(int col, int row) {
  PrepUi& p = *prep_;
  p.screen = PrepUi::Screen::Trade;
  itemPanel_.close();
  const Database& db = session_.db;
  const RosterUnit* a = member(*p.pt, p.who);
  if (!a) { prepItemsWhoOpen(); return; }
  const int size = db.rules.items.inventorySize;
  std::vector<PanelRow> left = grantRows(battle_, a->items, size), right;
  std::string rightName;
  if (p.with.empty()) {
    rightName = PrepWords::kStorage;
    right = grantRows(battle_, prepStorage(*p.pt, p.map), 0);
    if (right.empty()) {
      PanelRow none;
      none.label = PrepWords::kStorageEmpty;
      none.blank = true;
      right.push_back(none);
    }
  } else {
    const RosterUnit* b = member(*p.pt, p.with);
    if (!b) { prepItemsWhoOpen(); return; }
    rightName = b->name;
    right = grantRows(battle_, b->items, size);
  }
  tradePanel_.open(a->name, left, rightName, right, col, row);
  std::string log;
  for (const PanelRow& r : left) log += (log.empty() ? "" : ", ") + r.label;
  OTE_INFO("prep trade: %s [%s] with %s", a->name.c_str(), log.c_str(), rightName.c_str());
}

void BattleScene::prepTradePicked() {
  PrepUi& p = *prep_;
  const Database& db = session_.db;
  const int row = tradePanel_.row(), col = tradePanel_.col();
  const RosterUnit* a = member(*p.pt, p.who);
  if (!a) return;
  std::string why;
  bool ok;
  if (col == 0) {
    if (row >= (int)a->items.size()) { say(PrepWords::kNothingToHand); return; }
    ok = p.with.empty() ? prepStore(db, *p.pt, p.map, p.who, row, why) : prepGive(db, *p.pt, p.map, p.who, row, p.with, why);
  } else if (p.with.empty()) {
    if (row >= (int)prepStorage(*p.pt, p.map).size()) { say(PrepWords::kNothingToTake); return; }
    ok = prepFetch(db, *p.pt, p.map, p.who, row, why);
  } else {
    const RosterUnit* b = member(*p.pt, p.with);
    if (!b || row >= (int)b->items.size()) { say(PrepWords::kNothingToTake); return; }
    ok = prepGive(db, *p.pt, p.map, p.with, row, p.who, why);
  }
  if (!ok) {
    uiCue(Cue::Error);
    say(why);
    OTE_INFO("prep refused: %s", why.c_str());
    return;
  }
  uiCue(Cue::Confirm);
  prepChanged();
  prepTradeOpen(col, std::min(row, std::max(0, tradePanel_.row())));
}

// ---- the shop

void BattleScene::prepShopOpen(int sel) {
  PrepUi& p = *prep_;
  p.screen = PrepUi::Screen::Shop;
  menu_.close();
  const Database& db = session_.db;
  const std::vector<int> have = prepResources(db, *p.pt, p.map);
  std::vector<PanelRow> rows;
  p.shopItems = p.map.prep->shop;
  for (int i : p.shopItems) {
    const ItemDef& d = db.items[(size_t)i];
    ItemStack s;
    s.item = i;
    s.uses = d.uses;
    PanelRow row;
    row.label = d.name;
    row.extra = priceText(db, d.price);
    row.detail = itemDetail(battle_, s);
    for (const Amount& a : d.price) row.dim = row.dim || a.resource >= (int)have.size() || have[(size_t)a.resource] < a.amount;
    rows.push_back(row);
  }
  itemPanel_.open(PrepWords::kShop, fundsText(db, have), rows, PrepWords::kNothingForSale, sel);
  OTE_INFO("prep shop: %s (%s)", fundsText(db, have).c_str(), std::to_string(rows.size()).c_str());
}

// Who gets the item bought: a character, or the storage.
void BattleScene::prepShopForOpen() {
  PrepUi& p = *prep_;
  p.screen = PrepUi::Screen::ShopFor;
  std::vector<PanelRow> rows;
  p.ids.clear();
  for (const RosterUnit& r : p.pt->roster) {
    if (r.gone) continue;
    PanelRow row;
    row.label = r.name;
    row.extra = countText(session_.db, r);
    row.dim = (int)r.items.size() >= session_.db.rules.items.inventorySize && session_.db.rules.items.storage == ItemStorage::Off;
    rows.push_back(row);
    p.ids.push_back(r.id);
  }
  if (session_.db.rules.items.storage != ItemStorage::Off) {
    PanelRow row;
    row.label = PrepWords::kStorage;
    rows.push_back(row);
    p.ids.push_back("");
  }
  itemPanel_.open(PrepWords::kGiveTo, session_.db.items[(size_t)p.shopItem].name, rows, PrepWords::kNobody, 0);
}

// ---- input

void BattleScene::prepItemsInput(const InputEvent& e) {
  PrepUi& p = *prep_;
  Renderer& r = host_.renderer();
  switch (p.screen) {
    case PrepUi::Screen::ItemsWho: {
      ItemPanel::Result res = itemPanel_.onInput(e, r);
      if (res == ItemPanel::Back) prepMainMenu();
      else if (res == ItemPanel::Picked && itemPanel_.sel() < (int)p.ids.size()) {
        p.who = p.ids[(size_t)itemPanel_.sel()];
        prepItemsWithOpen();
      }
      break;
    }
    case PrepUi::Screen::ItemsWith: {
      ItemPanel::Result res = itemPanel_.onInput(e, r);
      if (res == ItemPanel::Back) prepItemsWhoOpen();
      else if (res == ItemPanel::Picked && itemPanel_.sel() < (int)p.ids.size()) {
        p.with = p.ids[(size_t)itemPanel_.sel()];
        prepTradeOpen();
      }
      break;
    }
    case PrepUi::Screen::Trade: {
      TradePanel::Result res = tradePanel_.onInput(e, r);
      if (res == TradePanel::Back) { tradePanel_.close(); prepItemsWithOpen(); }
      else if (res == TradePanel::Picked) prepTradePicked();
      break;
    }
    case PrepUi::Screen::Shop: {
      ItemPanel::Result res = itemPanel_.onInput(e, r);
      if (res == ItemPanel::Back) prepMainMenu();
      else if (res == ItemPanel::Picked && itemPanel_.sel() < (int)p.shopItems.size()) {
        p.shopItem = p.shopItems[(size_t)itemPanel_.sel()];
        const ItemDef& d = session_.db.items[(size_t)p.shopItem];
        const std::vector<int> have = prepResources(session_.db, *p.pt, p.map);
        bool pay = true;
        for (const Amount& a : d.price) pay = pay && a.resource < (int)have.size() && have[(size_t)a.resource] >= a.amount;
        if (!pay) { uiCue(Cue::Error); say("Not enough to pay for " + d.name); }
        else prepShopForOpen();
      }
      break;
    }
    case PrepUi::Screen::ShopFor: {
      ItemPanel::Result res = itemPanel_.onInput(e, r);
      if (res == ItemPanel::Back) prepShopOpen();
      else if (res == ItemPanel::Picked && itemPanel_.sel() < (int)p.ids.size()) {
        std::string why;
        const int row = itemPanel_.sel();
        if (!prepBuy(session_.db, *p.pt, p.map, p.ids[(size_t)row], p.shopItem, why)) {
          uiCue(Cue::Error);
          say(why);
          OTE_INFO("prep refused: %s", why.c_str());
        } else {
          uiCue(Cue::Confirm);
          say(std::string(PrepWords::kBought) + session_.db.items[(size_t)p.shopItem].name);
          OTE_INFO("prep: bought %s for %s", session_.db.items[(size_t)p.shopItem].id.c_str(), p.ids[(size_t)row].c_str());
          prepChanged();
          int at = 0;
          for (size_t i = 0; i < p.shopItems.size(); ++i)
            if (p.shopItems[i] == p.shopItem) at = (int)i;
          prepShopOpen(at);
        }
      }
      break;
    }
    default: break;
  }
}

}  // namespace ote
