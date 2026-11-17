// Words about staves for the action menu, the Staff list, the forecast box and the battle's floating texts (docs/SRPG.md
// section 8). ALL placeholder wording of that part of the player lives here (the owner reviews it in one place). No scene,
// no drawing: unit-testable.
#pragma once
#include <string>
#include <vector>

#include "core/battle/battle.h"

namespace ote {

struct StaffWords {
  static constexpr const char* kMenu = "Staff";            // the action menu entry, and the title of the list
  static constexpr const char* kMissed = "Missed";          // floats over a unit the staff missed
  static constexpr const char* kBreaks = "Breaks";          // forecast: this is its last use
  static constexpr const char* kNobody = "Nobody in reach needs it";
  static constexpr const char* kNobodyShort = "Nobody needs it";
  static constexpr const char* kNobodyTiny = "No one";
  static constexpr const char* kCannotWield = "Cannot wield";
  static constexpr const char* kNothingToTarget = "Nobody to help";  // hint line when a list entry is chosen with no target
  static constexpr const char* kMore = " more";                // forecast: "+2 more"
  // Stage 2 (docs/SRPG.md section 8): moving units, repairing, lighting. All placeholder wording.
  static constexpr const char* kPickSquare = "Pick a square to send it to";  // hint when a warp asks for its square
  static constexpr const char* kPickLight = "Pick a square to light";         // hint when a light staff asks for its square
  static constexpr const char* kRepairWhich = "Repair which?";                // title of the menu of items to repair
  static constexpr const char* kPickItemNote = "Pick an item to repair";      // forecast line while the item is still to pick
  static constexpr const char* kPickSquareNote = "pick a square";             // forecast line (after the unit's name) for a warp
  static constexpr const char* kWarpsThere = "warps there";                   // forecast: "Tarn warps there"
  static constexpr const char* kPulledToYou = "is pulled next to you";        // forecast: "Tarn is pulled next to you"
  static constexpr const char* kSwapPlaces = "swap places";                   // forecast: "Tarn and Wen swap places"
  static constexpr const char* kLights = "Lights";                            // forecast: "Lights 12 new squares"
  static constexpr const char* kNewSquares = "new squares";
  static constexpr const char* kNewSquare = "new square";
  static constexpr const char* kTurn = "turn";                                // forecast: "2 turns"
  static constexpr const char* kTurns = "turns";
  static constexpr const char* kBlocked = "Blocked!";                         // floats when a warp ran into an enemy nobody had seen
  static constexpr const char* kLit = "Lit";                               // floats over a lit square
  static constexpr const char* kOutOfRange = "Out of range";                  // a tapped square or unit that is too far
  static constexpr const char* kNotAValidTarget = "Not a valid target";
  static constexpr const char* kCannotSee = "Cannot see that unit";
  static constexpr const char* kNothingToDo = "Nothing it would do";
  static constexpr const char* kAlreadyBeside = "Already next to you";
  static constexpr const char* kNoRoomBeside = "No room next to you";
  static constexpr const char* kNoFreeSquare = "No free square in reach";
  static constexpr const char* kCannotStandThere = "Cannot stand there";
  static constexpr const char* kNothingToRepair = "Nothing to repair";
  static constexpr const char* kOccupied = "That square is taken";
  static constexpr const char* kNotInSight = "Not in sight";
  static constexpr const char* kNothingHidden = "Nothing hidden there";
};

// What the core's reason (Battle::staffOptions) reads like to a player. Form 0 (full): "Needs rank C", "Only Mara can wield
// this", "Cannot wield", "Nobody in reach needs it". Form 1 (short, for a menu with little room): "Needs rank C", "Only Mara",
// "Cannot wield", "Nobody needs it". Form 2 (tiny, a phone): "Rank C", "Locked", "Class", "No one". A text that is none of
// these comes back as it was.
std::string staffReasonText(const std::string& coreReason, int form = 0);

// What the staff does, in one phrase for the hint line: "Heals 8 HP", "Heals all HP", "Heals 25%" (the first effect of the
// item; "Heals" alone when a formula decides it); "Warps an ally", "Pulls it next to you", "Swaps places", "Repairs an item",
// "Lights an area".
std::string staffEffectText(const Database& db, const ItemDef& item);

// What the core says about a unit or a square that cannot be picked (Battle::staffTargetWhy), in the player's words.
std::string staffWhyText(const std::string& coreReason);

// The lines of the forecast box about one unit that is hit, beyond its HP: where it goes, the item it gets back ("Iron Blade 3 > 8").
std::vector<std::string> staffMoveLines(const Battle& b, const StaffForecast& f, const StaffHit& h);
// The lines of a light staff's forecast: "Lights 5 new squares" and "radius 2, 2 turns".
std::vector<std::string> staffLightLines(const StaffForecast& f);

// "Hit 60%" / "Hit 60-80%" for the forecast: empty when the staff cannot miss.
std::string staffHitText(const StaffForecast& f);

}  // namespace ote
