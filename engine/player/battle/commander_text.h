// Words and small helpers about commanders for the player (docs/COMMANDERS.md "Player"): the map menu's power entries, the meter in the side panel, the
// banner of a power, and the lines of the commander pick screen. ALL placeholder wording of that part lives here (commander_text.cpp).
#pragma once
#include <string>
#include <vector>

#include "core/battle/battle.h"

namespace ote {

// Menu entry ids of the end (map) menu: kPowerBase + the power's index. Above every id of srpg_menu_ids.h.
constexpr int kPowerBase = 6000;

struct CommanderWords {
  static constexpr const char* kPowerEntry = "Power: ";
  static constexpr const char* kActive = "active";
  static constexpr const char* kNeeds = "needs ";  // "needs 12 more"
  static constexpr const char* kMore = " more";
  static constexpr const char* kPassive = "Passive: ";
  static constexpr const char* kMeter = "Meter: ";
  static constexpr const char* kPowerWord = "Power";
  static constexpr const char* kEnded = " ended";
  static constexpr const char* kHintReady = " (power ready)";
  static constexpr const char* kHintReadySpeed = "/power";
  static constexpr const char* kPickTitleSuffix = " - commander";
};

// "12/40": the side's meter and its top ("" for a side without a commander).
std::string meterText(const Battle& b, int side);
// "Power: Rally" for the map menu; `extra` is the price ("40"), "needs 12 more" while the meter is short, or "active" while a power runs;
// `enabled` says whether it can be used now.
std::string powerEntryLabel(const Battle& b, int side, int power);
std::string powerEntryExtra(const Battle& b, int side, int power, bool& enabled);
// Is any power of the side's commander usable now (a person's turn is assumed by the caller)?
bool anyPowerReady(const Battle& b, int side);
// What a power does, in words: its description, or when it has none a summary made of its parts ("+3 Strength ..." is the skill's name: "Frenzy for 1 turn;
// heals 5").
std::string powerSummary(const Database& db, const CommanderPower& p);
// The lines of the pick screen under the list for one commander (description first, then the passive skills, each power with its price, the meter rule).
std::vector<std::string> commanderLines(const Database& db, const CommanderDef& d);

}  // namespace ote
