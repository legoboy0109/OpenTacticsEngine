// Words about doors, chests and visiting for the action menu, the pick, the forecast box, the info panel and the battle's
// floating texts (docs/SRPG.md section 10 "Player"). ALL placeholder wording of that part of the player lives here (the owner
// reviews it in one place). No scene, no drawing: unit-testable.
#pragma once
#include <string>

#include "core/battle/battle.h"

namespace ote {

struct PlaceWords {
  // Action menu
  static constexpr const char* kOpen = "Open";
  static constexpr const char* kVisit = "Visit";
  static constexpr const char* kDestroy = "Destroy";
  static constexpr const char* kBreak = "Break door";             // the menu entry that picks a door to attack
  static constexpr const char* kWhichKey = "Which key?";          // title of the menu of keys when several fit
  static constexpr const char* kNeedsKey = "Needs a key";         // greyed reasons (menu: short forms)
  static constexpr const char* kLockedShort = "Locked";
  static constexpr const char* kPackFull = "Pack full";
  static constexpr const char* kUnlock = "Unlock";                // used in hints only; the ability carries its own name
  // Hints while a square is picked
  static constexpr const char* kPickDoor = "Pick a door to break";
  static constexpr const char* kPickUnlock = "Pick a door or chest to unlock";
  static constexpr const char* kNotBreakable = "Not a door you can break";
  static constexpr const char* kNothingToUnlock = "Nothing to unlock";
  // Forecast box
  static constexpr const char* kDamage = "Damage";
  static constexpr const char* kStrikes = "strikes";
  static constexpr const char* kHp = "HP";
  static constexpr const char* kBreaks = "Breaks";                // the door opens (hit points run out)
  static constexpr const char* kOpens = "Opens";                  // unlock staff: "Opens Iron Gate"
  // Floating texts
  static constexpr const char* kFloatOpens = " opens";            // "Iron Gate opens"
  static constexpr const char* kFloatShuts = " shuts";
  static constexpr const char* kFloatVisited = "Visited";
  static constexpr const char* kFloatDestroyed = "Destroyed";
  static constexpr const char* kFloatUnlocked = "Unlocked";
  static constexpr const char* kFound = "Found ";                 // "Found Potion", "Found 50 Gold"
  // Info panel state rows
  static constexpr const char* kStateRow = "State";
  static constexpr const char* kStLocked = "Locked";
  static constexpr const char* kStShut = "Shut";
  static constexpr const char* kStOpen = "Open";
  static constexpr const char* kStVisited = "Visited";
  static constexpr const char* kStRuined = "Ruined";
  static constexpr const char* kStHp = "HP";
};

// The short reason for a greyed Open entry from the core's words ("it is locked and needs a key" -> "Needs a key").
std::string placeReasonText(const std::string& coreReason);

// The state rows of the info panel for a structure: {"State", "Locked"} and {"HP", "7/20"} pairs. `known` = the viewer sees the
// structure now (an unseen one shows only what its remembered kind implies: no lock, no hit points, no visited mark).
// `kind` is the kind to show (what the viewer last saw).
struct PlaceRow {
  std::string label, value;
};
std::vector<PlaceRow> placeInfoRows(const Battle& b, int structure, int kind, bool known);

}  // namespace ote
