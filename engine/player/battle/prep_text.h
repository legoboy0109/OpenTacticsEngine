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
  static constexpr const char* kPutIn = "{confirm}: put in the battle";
  static constexpr const char* kTakeOut = "{confirm}: take out of the battle";
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
  static constexpr const char* kHintMain = "{udok}:choose";
  static constexpr const char* kHintUnits = "{updown}  {ok}:in / out  {cancel}:back";
  static constexpr const char* kHintPick = "{dpad}  {ok}:pick up  {cancel}:back";
  static constexpr const char* kHintPlace = "{dpad}  {ok}:put here  {cancel}:keep in place";
  static constexpr const char* kHintView = "{dpad}:look  {ok}:unit info  {cancel}:back";
  static constexpr const char* kHintList = "{udok}:choose  {cancel}:back";
  static constexpr const char* kHintTrade = "{leftright}:unit  {updown}  {ok}:hand over  {cancel}:back";
  static constexpr const char* kHintBuy = "{udok}:buy  {cancel}:back";
  static constexpr const char* kHintBuyTabs = "{ok}:buy  {right}:Sell  {cancel}:back";
  static constexpr const char* kHintSellWho = "{ok}:choose  {left}:Buy  {cancel}:back";
  static constexpr const char* kHintSell = "{udok}:sell  {cancel}:back";
  static constexpr const char* kHintTradeEquip = "{leftright}:unit  {ok}:hand/equip  {cancel}:back";
  static constexpr const char* kHintAsk = "{lrok}:answer  {cancel}:back";
};

}  // namespace ote
