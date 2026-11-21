// Selling at the preparation shop and Equip on the trade screen (docs/EVENTS.md "Preparation"). The rules are the core's
// (prepSell, prepEquip in core/campaign/prep.h); this file only lists, asks and reports.
//
// The shop has two tabs, Buy (battle_prep_items.cpp) and Sell. Sell first asks whose items (every character, then the
// storage), then lists that pack with what the shop would pay for each item; choosing a row sells it at once (buying has
// no "are you sure" either, and the item list is the confirmation of what is being sold). Equip: choosing a weapon of
// the character on the left of the trade screen opens a small menu, Hand over / Equip, under the row; Equip is greyed
// with the reason when the weapon cannot be wielded. The weapon in hand needs no menu (nothing to equip).
#include "battle/battle_prep_ui.h"
#include "battle/item_text.h"
#include "battle/prep_text.h"
#include "battle/storage_stack.h"
#include "core/base/log.h"
#include "sound/cue_sink.h"
#include "ui/text.h"

namespace ote {
using namespace prepui;

namespace {

enum TradeChoiceId { kcHandOver = 1, kcEquip };

const std::vector<ItemGrant>& packOf(const Playthrough& pt, const MapDef& map, const std::string& id) {
  static const std::vector<ItemGrant> none;
  if (id.empty()) return prepStorage(pt, map);
  const RosterUnit* r = pt.findUnit(id);
  return r && !r->gone ? r->items : none;
}

std::string sellFromName(const Playthrough& pt, const std::string& id) {
  if (id.empty()) return PrepWords::kStorage;
  const RosterUnit* r = pt.findUnit(id);
  return r ? r->name : id;
}

// The rows of a pack's sell list: the storage stacks identical copies (storage_stack.h), a character's items stay one row each.
// `index` is the position in the pack that choosing the row sells.
std::vector<StorageRow> sellRows(const std::vector<ItemGrant>& pack, bool storage) {
  if (storage) return stackStorage(pack);
  std::vector<StorageRow> rows;
  for (size_t i = 0; i < pack.size(); ++i) rows.push_back({pack[i].item, pack[i].uses, pack[i].drop, 1, (int)i});
  return rows;
}

}  // namespace

// ---- the shop's tabs

void BattleScene::prepShopTab(int tab) {
  if (tab == 1) prepSellWhoOpen();
  else prepShopOpen();
}

// Whose items to sell: every character, then the storage when the game has one.
void BattleScene::prepSellWhoOpen(int sel) {
  PrepUi& p = *prep_;
  p.screen = PrepUi::Screen::SellWho;
  menu_.close();
  const Database& db = session_.db;
  std::vector<PanelRow> rows;
  p.ids.clear();
  for (const RosterUnit& r : p.pt->roster) {
    if (r.gone) continue;
    PanelRow row;
    row.label = r.name;
    int type = r.type, level = r.level;
    prepLive(r.id, type, level);
    decorateUnitRow(row, type, level);
    row.extra = countText(db, r);
    row.dim = r.items.empty();
    row.detail = std::string(PrepWords::kSellFrom) + " " + r.name;
    rows.push_back(row);
    p.ids.push_back(r.id);
  }
  if (db.rules.items.storage != ItemStorage::Off) {
    PanelRow row;
    row.label = PrepWords::kStorage;
    row.extra = intStr((int)prepStorage(*p.pt, p.map).size());
    row.dim = prepStorage(*p.pt, p.map).empty();
    row.detail = std::string(PrepWords::kSellFrom) + " " + PrepWords::kStorage;
    rows.push_back(row);
    p.ids.push_back("");
  }
  const std::string funds = fundsText(db, prepResources(db, *p.pt, p.map));
  itemPanel_.open(shopTitle(p.map.prep->shops[(size_t)p.shopIx]), funds, rows, PrepWords::kNobody, sel);
  itemPanel_.setTabs({PrepWords::kBuy, PrepWords::kSell}, 1);
  OTE_INFO("prep sell who: %s", funds.c_str());
  logPanel(itemPanel_, host_.renderer(), layout_.vw, layout_.vh, "sellwho");
}

// One pack with what each item would fetch.
void BattleScene::prepSellItemsOpen(int sel) {
  PrepUi& p = *prep_;
  p.screen = PrepUi::Screen::SellItems;
  const Database& db = session_.db;
  const std::vector<ItemGrant>& pack = packOf(*p.pt, p.map, p.sellFrom);
  std::vector<PanelRow> rows;
  std::string log;
  for (const StorageRow& g : sellRows(pack, p.sellFrom.empty())) {
    const ItemStack s = stackOf(pack[(size_t)g.index]);
    PanelRow row;
    row.label = battle_.itemOf(s).name;
    row.pic = itemicon::ofItem(db, s.item);
    row.detail = itemDetail(battle_, s);
    row.count = g.count;
    std::vector<Amount> value;
    std::string why;
    if (prepSellValue(db, pack[(size_t)g.index], value, why)) {
      row.extra = priceText(db, value);
    } else {  // not bought: greyed, and the reason where the explanation goes
      row.extra = "-";
      row.dim = true;
      row.detail = why;
    }
    log += (log.empty() ? "" : ", ") + row.label + (g.count > 1 ? " x" + intStr(g.count) : "") + " [" + row.extra + "]";
    rows.push_back(row);
  }
  const std::string funds = fundsText(db, prepResources(db, *p.pt, p.map));
  itemPanel_.open(std::string(PrepWords::kSell) + ": " + sellFromName(*p.pt, p.sellFrom), funds, rows, PrepWords::kNothingToSell, sel);
  OTE_INFO("prep sell: %s [%s] (%s)", sellFromName(*p.pt, p.sellFrom).c_str(), log.c_str(), funds.c_str());
  logPanel(itemPanel_, host_.renderer(), layout_.vw, layout_.vh, "sell");
}

void BattleScene::prepSellPicked() {
  PrepUi& p = *prep_;
  const Database& db = session_.db;
  const int row = itemPanel_.sel();
  const std::vector<ItemGrant>& pack = packOf(*p.pt, p.map, p.sellFrom);
  const std::vector<StorageRow> rows = sellRows(pack, p.sellFrom.empty());
  if (row < 0 || row >= (int)rows.size()) return;
  const int slot = rows[(size_t)row].index;  // one copy: the first of the row's identical ones
  const ItemGrant g = pack[(size_t)slot];  // copied: the pack changes below
  std::vector<Amount> value;
  std::string why;
  prepSellValue(db, g, value, why);
  if (!prepSell(db, *p.pt, p.map, p.sellFrom, slot, why, p.shopIx)) {
    uiCue(Cue::Error);
    say(why);
    OTE_INFO("prep refused: %s", why.c_str());
    return;
  }
  const std::string name = db.items[(size_t)g.item].name, got = priceText(db, value);
  uiCue(Cue::Confirm);
  say(std::string(PrepWords::kSold) + name + PrepWords::kFor + got);
  OTE_INFO("prep: sold %s from %s for %s", db.items[(size_t)g.item].id.c_str(), p.sellFrom.empty() ? "storage" : p.sellFrom.c_str(),
           got.c_str());
  prepChanged();
  prepSellItemsOpen(std::max(0, std::min(row, (int)sellRows(packOf(*p.pt, p.map, p.sellFrom), p.sellFrom.empty()).size() - 1)));
}

// ---- Equip

// A weapon of the character on the left that is not already in hand: Hand over / Equip.
bool BattleScene::prepTradeNeedsChoice(int row) const {
  const PrepUi& p = *prep_;
  const Database& db = session_.db;
  const RosterUnit* a = p.pt->findUnit(p.who);
  if (!db.rules.items.carriedWeapons || !a || row < 0 || row >= (int)a->items.size()) return false;
  return db.items[(size_t)a->items[(size_t)row].item].kind == ItemKind::Weapon && prepEquippedAt(db, *p.pt, p.who) != row;
}

void BattleScene::prepTradeChoiceOpen(int row) {
  PrepUi& p = *prep_;
  p.screen = PrepUi::Screen::TradeChoice;
  p.tradeRow = row;
  std::string why;
  const bool can = prepEquipReason(session_.db, *p.pt, p.who, row, why);
  std::vector<MenuItem> items;
  items.push_back({p.with.empty() ? PrepWords::kPutAway : PrepWords::kHandOver, "", true, kcHandOver});
  items.push_back({PrepWords::kEquip, can ? "" : why, can, kcEquip});  // greyed, with the reason
  if (!can) say(why);  // the menu's own line is short: the whole reason here
  const RosterUnit* a = p.pt->findUnit(p.who);
  Renderer& r = host_.renderer();
  menu_.open(r, session_.db.items[(size_t)a->items[(size_t)row].item].name, items, 0, 0, layout_.vw, layout_.vh);
  int x, y, w, h;
  tradePanel_.selectedBox(r, x, y, w, h);
  menu_.place(x + w - menu_.w(), y + h, layout_.vw, layout_.vh);  // under the row, so the row stays readable
  std::string log;
  for (const MenuItem& m : items) log += (log.empty() ? "" : ", ") + m.label + (m.enabled ? "" : " [" + m.extra + "]");
  OTE_INFO("prep item menu: %s", log.c_str());
  std::string rows;  // where each entry is on screen: the screen tests tap them
  for (size_t i = 0; i < items.size(); ++i) {
    int cx, cy;
    menu_.rowCentre((int)i, cx, cy);
    rows += " " + intStr((int)i) + "@" + intStr(cx) + "," + intStr(cy);
  }
  OTE_INFO("prep item menu rows:%s", rows.c_str());
}

void BattleScene::prepTradeChoice(int id) {
  PrepUi& p = *prep_;
  const Database& db = session_.db;
  menu_.close();
  p.screen = PrepUi::Screen::Trade;
  if (id == kcHandOver) {
    prepTradePicked(true);
    return;
  }
  std::string why;
  const RosterUnit* a = p.pt->findUnit(p.who);
  const std::string name =
      a && p.tradeRow >= 0 && p.tradeRow < (int)a->items.size() ? db.items[(size_t)a->items[(size_t)p.tradeRow].item].name : "";
  if (!prepEquip(db, *p.pt, p.map, p.who, p.tradeRow, why)) {
    uiCue(Cue::Error);
    say(why);
    OTE_INFO("prep refused: %s", why.c_str());
    return;
  }
  uiCue(Cue::Confirm);
  say(PrepWords::kEquipped + name);
  OTE_INFO("prep: equipped %s for %s", name.c_str(), p.who.c_str());
  prepChanged();
  prepTradeOpen(0, 0);  // the weapon is now the first item
}

// ---- input

void BattleScene::prepSellInput(const InputEvent& e) {
  PrepUi& p = *prep_;
  Renderer& r = host_.renderer();
  switch (p.screen) {
    case PrepUi::Screen::TradeChoice: {
      Menu::Result res = menu_.onInput(e);
      if (res == Menu::Cancelled) {
        menu_.close();
        p.screen = PrepUi::Screen::Trade;
      } else if (res == Menu::Chosen) {
        prepTradeChoice(menu_.chosenId());
      }
      break;
    }
    case PrepUi::Screen::SellWho: {
      ItemPanel::Result res = itemPanel_.onInput(e, r);
      if (res == ItemPanel::Back) prepMainMenu();
      else if (res == ItemPanel::Tab) prepShopTab(itemPanel_.tab());
      else if (res == ItemPanel::Picked && itemPanel_.sel() < (int)p.ids.size()) {
        p.sellFrom = p.ids[(size_t)itemPanel_.sel()];
        prepSellItemsOpen();
      }
      break;
    }
    case PrepUi::Screen::SellItems: {
      ItemPanel::Result res = itemPanel_.onInput(e, r);
      if (res == ItemPanel::Back) {  // back to whose items, on the same row
        int at = 0;
        for (size_t i = 0; i < p.ids.size(); ++i)
          if (p.ids[i] == p.sellFrom) at = (int)i;
        prepSellWhoOpen(at);
      } else if (res == ItemPanel::Picked) {
        prepSellPicked();
      }
      break;
    }
    default: break;
  }
}

}  // namespace ote
