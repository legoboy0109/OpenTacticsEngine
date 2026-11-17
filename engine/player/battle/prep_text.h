// ALL placeholder wording of the preparation screen in the player (docs/EVENTS.md "Preparation") in one place, for the owner
// to review. No scene, no drawing. (The reasons a character is locked in or out come from the core: prepLockReason and the
// refusal sentences of core/campaign/prep.cpp.)
#pragma once
#include <string>

namespace ote {

struct PrepWords {
  // Main menu
  static constexpr const char* kTitle = "Preparation";
  // The screen's title; a campaign with parties adds the active party's name ("Preparation - The northern road").
  static std::string title(const std::string& party) { return party.empty() ? std::string(kTitle) : std::string(kTitle) + " - " + party; }
  static constexpr const char* kUnits = "Units";
  static constexpr const char* kPositions = "Positions";
  static constexpr const char* kItems = "Items";
  static constexpr const char* kShop = "Shop";
  static constexpr const char* kView = "View map";
  static constexpr const char* kSave = "Save";
  static constexpr const char* kLeave = "Leave";
  static constexpr const char* kAbandon = "Abandon";  // a free battle: nothing is kept
  static constexpr const char* kBegin = "Begin";
  // Units
  static constexpr const char* kWhoFights = "Who fights";
  static constexpr const char* kDeployed = "Deployed";   // "Deployed 2 / 3"
  static constexpr const char* kSlot = "Slot";           // "Slot 2"
  static constexpr const char* kOut = "Out";
  static constexpr const char* kPutIn = "Confirm: put in the battle";
  static constexpr const char* kTakeOut = "Confirm: take out of the battle";
  static constexpr const char* kNobody = "Nobody to choose from.";
  // Positions
  static constexpr const char* kNotASlot = "Not a deployment square";
  static constexpr const char* kNobodyThere = "Nobody is deployed there";
  // Items
  static constexpr const char* kWhoseItems = "Whose items?";
  static constexpr const char* kTradeWith = "Trade with";
  static constexpr const char* kStorage = "Storage";
  static constexpr const char* kStorageEmpty = "The storage is empty.";
  static constexpr const char* kNothingToHand = "Nothing to hand over";
  static constexpr const char* kNothingToTake = "Nothing to take";
  static constexpr const char* kNoOneToTrade = "There is nobody to trade with.";
  // Shop
  static constexpr const char* kNothingForSale = "Nothing for sale.";
  static constexpr const char* kGiveTo = "Give to";
  static constexpr const char* kBought = "Bought ";
  static constexpr const char* kBuy = "Buy";              // the shop's tabs
  static constexpr const char* kSell = "Sell";
  static constexpr const char* kSellFrom = "Sell from";   // whose items (or the storage) to sell
  static constexpr const char* kSold = "Sold ";           // "Sold Potion for 10 Gold"
  static constexpr const char* kFor = " for ";
  static constexpr const char* kNothingToSell = "Nothing to sell.";
  // Equip, on a character's item list
  static constexpr const char* kHandOver = "Hand over";
  static constexpr const char* kPutAway = "Put in storage";
  static constexpr const char* kEquip = "Equip";
  static constexpr const char* kEquipped = "Equipped ";   // "Equipped Axe"
  // Begin
  static constexpr const char* kBeginAsk = "Begin with ";  // "Begin with 2 of 3?"
  static constexpr const char* kOf = " of ";
  static constexpr const char* kBack = "Back";
  // Saving
  static constexpr const char* kSavedTo = "Saved to slot ";
  static constexpr const char* kLeaveNote = "You will leave the preparation.";
  // Hints
  static constexpr const char* kHintMain = "Up/Down + Z:choose";
  static constexpr const char* kHintUnits = "Up/Down  Z:in / out  X:back";
  static constexpr const char* kHintPick = "Arrows  Z:pick up  X:back";
  static constexpr const char* kHintPlace = "Arrows  Z:put here  X:keep in place";
  static constexpr const char* kHintView = "Arrows:look  Z:unit info  X:back";
  static constexpr const char* kHintList = "Up/Down + Z:choose  X:back";
  static constexpr const char* kHintTrade = "Left/Right:unit  Up/Down  Z:hand over  X:back";
  static constexpr const char* kHintBuy = "Up/Down + Z:buy  X:back";
  static constexpr const char* kHintBuyTabs = "Z:buy  Right:Sell  X:back";
  static constexpr const char* kHintSellWho = "Z:choose  Left:Buy  X:back";
  static constexpr const char* kHintSell = "Up/Down + Z:sell  X:back";
  static constexpr const char* kHintTradeEquip = "Left/Right:unit  Z:hand/equip  X:back";
  static constexpr const char* kHintAsk = "Left/Right + Z:answer  X:back";
};

}  // namespace ote
