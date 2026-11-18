// What the preparation screen of the battle scene remembers (battle_prep*.cpp; docs/EVENTS.md "Preparation"). Private to
// those files: the scene only holds a pointer to it.
#pragma once
#include <string>
#include <vector>

#include "battle/battle_scene.h"
#include "core/campaign/prep.h"

namespace ote {

struct BattleScene::PrepUi {
  enum class Screen { Main, Units, Positions, ItemsWho, ItemsWith, Trade, TradeChoice, Shop, ShopFor, SellWho, SellItems, View, Begin };
  Screen screen = Screen::Main;
  MapDef map;                   // the chapter's (or the free battle's) map, as the loader read it
  Playthrough* pt = nullptr;    // the playthrough in play, or the scratch one of a free battle
  bool campaign = false;        // a chapter of a story: the roster, the storage and the shop exist
  int carrySlot = -1;           // Positions: the slot whose character is picked up (-1 = none)
  std::vector<std::string> ids; // character ids of the rows of the list that is open (Units, ItemsWho, ItemsWith, ShopFor); "" = the storage
  std::vector<int> shopItems;   // item index of each row of the shop list
  std::string who, with;        // Trade: the two sides ("" with = the storage)
  int shopItem = -1;            // the item picked in the shop, being given to someone
  int shopIx = 0;               // which of the map's shops is open (PrepDef::shops)
  std::string sellFrom;         // SellItems: whose items are listed ("" = the storage)
  int tradeRow = -1;            // TradeChoice: the position of the item the small menu is about
  int overlayUnit = -1;         // View: the enemy whose reach is drawn
};

// Small helpers shared by battle_prep_items.cpp and battle_prep_sell.cpp.
namespace prepui {
// Logs "prep panel <what>: row0@x,y step=h tab0@x,y tab1@x,y": where the first row, the row height and the tabs of the open
// list panel are on screen (the screen tests tap them). Assumes the list is not scrolled.
void logPanel(ItemPanel& panel, Renderer& r, int vw, int vh, const char* what);
ItemStack stackOf(const ItemGrant& g);
// The panel title of a shop: its name, or the plain word for the single unnamed one.
inline std::string shopTitle(const PrepShop& s) { return s.name.empty() ? std::string("Shop") : s.name; }
std::string countText(const Database& db, const RosterUnit& r);  // "2/3": items carried over the most it can carry
std::string fundsText(const Database& db, const std::vector<int>& have);  // "Gold 125"
}  // namespace prepui

}  // namespace ote
