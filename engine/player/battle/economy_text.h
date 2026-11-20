// Words about upgrades, research, fuel and ammunition shared by the structure menus, the research panel, the
// info panel and the event banners (docs/ECONOMY.md "Player"). ALL placeholder wording of that part of the
// player lives in EconomyWords and economy_text.cpp: nothing else spells these strings out.
#pragma once
#include <string>
#include <vector>

#include "core/battle/battle.h"
#include "ui/item_panel.h"

namespace ote {

struct EconomyWords {
  // Entries of the structure menu and the action menu.
  static constexpr const char* kBuild = "Build";
  static constexpr const char* kUpgrade = "Upgrade";
  static constexpr const char* kResearch = "Research";
  static constexpr const char* kSupply = "Supply";
  // Research panel.
  static constexpr const char* kNoTechnologies = "No technologies.";
  static constexpr const char* kDone = "done";
  static constexpr const char* kFree = "free";
  static constexpr const char* kInstant = "instant";
  static constexpr const char* kPaused = "paused";
  static constexpr const char* kPausedWhy = "paused: no research structure";
  static constexpr const char* kUnlocks = "Unlocks: ";
  static constexpr const char* kAllows = "Allows: ";
  static constexpr const char* kForAll = " for all units";
  static constexpr const char* kFor = " for ";
  static constexpr const char* kInProgress = "In progress. ";
  // Branching research.
  static constexpr const char* kTier = "tier ";
  static constexpr const char* kOtherTree = "Other";
  static constexpr const char* kCloses = "Closes: ";
  static constexpr const char* kPerTurn = " per turn";
  static constexpr const char* kSkill = "Skill: ";
  static constexpr const char* kCancelResearch = "Cancel research";
  static constexpr const char* kKeepResearching = "Keep researching";
  static constexpr const char* kNoRefund = "no refund";
  static constexpr const char* kRefund = "+";
  static constexpr const char* kBannerCancelled = "cancelled ";
  static constexpr const char* kHintCancelResearch = "{udok}:choose  {cancel}:back";
  static constexpr const char* kHintResearchCancel = "{udok}:select  {cancel}:back";
  // Info panel.
  static constexpr const char* kFuel = "Fuel";
  static constexpr const char* kEmpty = "EMPTY";
  // Hint line, notices, floating texts, banners.
  static constexpr const char* kHintStructure = "{udok}:choose  {cancel}:cancel";
  static constexpr const char* kHintUpgrade = "{udok}:upgrade  {cancel}:back";
  static constexpr const char* kHintResearch = "{udok}:research  {cancel}:back";
  static constexpr const char* kLowFuel = "Low fuel: move limited to ";
  static constexpr const char* kAlreadyResearching = "Already researching ";
  static constexpr const char* kFloatUpgraded = "Upgraded: ";
  static constexpr const char* kFloatResupplied = "Resupplied";
  static constexpr const char* kBannerResearched = " researched";
  static constexpr const char* kBannerResearching = "researching ";
};

// "1 turn" / "3 turns".
std::string turnsText(int n);
// "200 Funds, 2 turns" (the cost may be empty: "free"; no turns: "instant").
std::string techPrice(const Database& db, int tech);
// What a technology gives, built from its data: "Unlocks: Tank; Allows: Town; +1 Def for Infantry".
std::string techGives(const Database& db, int tech);
// The research panel's rows for `side`: the one in progress first, then what can be started (greyed with the
// reason when not), then the finished ones. `techOfRow` gets the technology index of each row.
std::vector<PanelRow> researchRows(const Battle& b, int side, std::vector<int>& techOfRow);
// True when a technology names a "tree" or a "tier": the panel then groups the rows (research_rows.cpp).
bool researchIsGrouped(const Database& db);
std::vector<PanelRow> groupedResearchRows(const Battle& b, int side, std::vector<int>& techOfRow);  // headings have techOfRow -1
// The selected row's explanation: `lead` (a reason or "In progress."), the description, what it gives, the branches it closes.
std::string techDetail(const Database& db, int tech, const std::string& lead);
// "Masonry" and "2 turns left" / "paused" for the side's research in progress (empty when none).
std::string researchName(const Battle& b, int side);
std::string researchLeft(const Battle& b, int side);
// "34/60"; empty for a unit that uses no fuel.
std::string fuelValue(const Battle& b, const Unit& u);
bool fuelLow(const Battle& b, const Unit& u);  // a quarter or less of the tank (what the AI treats as low)
// The fixed weapons of the unit that have ammunition: name, "3/6", whether empty.
struct AmmoLine {
  std::string name, value;
  bool empty = false;
};
std::vector<AmmoLine> ammoLines(const Battle& b, const Unit& u);
// One line for the log: "Tank: Fuel 34/60, Cannon 3/6, MG 0/6 EMPTY" (empty when the unit has neither).
std::string supplyLogLine(const Battle& b, const Unit& u);
// A panel row as text for the log: "Masonry (200 Funds, 2 turns) [greyed]".
std::string rowLogText(const PanelRow& r);

}  // namespace ote
