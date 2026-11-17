// What the preparation screen of the battle scene remembers (battle_prep*.cpp; docs/EVENTS.md "Preparation"). Private to
// those files: the scene only holds a pointer to it.
#pragma once
#include <string>
#include <vector>

#include "battle/battle_scene.h"
#include "core/campaign/prep.h"

namespace ote {

struct BattleScene::PrepUi {
  enum class Screen { Main, Units, Positions, ItemsWho, ItemsWith, Trade, Shop, ShopFor, View, Begin };
  Screen screen = Screen::Main;
  MapDef map;                   // the chapter's (or the free battle's) map, as the loader read it
  Playthrough* pt = nullptr;    // the playthrough in play, or the scratch one of a free battle
  bool campaign = false;        // a chapter of a story: the roster, the storage and the shop exist
  int carrySlot = -1;           // Positions: the slot whose character is picked up (-1 = none)
  std::vector<std::string> ids; // character ids of the rows of the list that is open (Units, ItemsWho, ItemsWith, ShopFor); "" = the storage
  std::vector<int> shopItems;   // item index of each row of the shop list
  std::string who, with;        // Trade: the two sides ("" with = the storage)
  int shopItem = -1;            // the item picked in the shop, being given to someone
  int overlayUnit = -1;         // View: the enemy whose reach is drawn
};

}  // namespace ote
