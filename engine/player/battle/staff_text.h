// Words about staves for the action menu, the Staff list, the forecast box and the battle's floating texts (docs/SRPG.md
// section 8). ALL placeholder wording of that part of the player lives here (the owner reviews it in one place). No scene,
// no drawing: unit-testable.
#pragma once
#include <string>

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
};

// What the core's reason (Battle::staffOptions) reads like to a player. Form 0 (full): "Needs rank C", "Only Mara can wield
// this", "Cannot wield", "Nobody in reach needs it". Form 1 (short, for a menu with little room): "Needs rank C", "Only Mara",
// "Cannot wield", "Nobody needs it". Form 2 (tiny, a phone): "Rank C", "Locked", "Class", "No one". A text that is none of
// these comes back as it was.
std::string staffReasonText(const std::string& coreReason, int form = 0);

// What the staff does, in one phrase for the hint line: "Heals 8 HP", "Heals all HP", "Heals 25%" (the first heal effect
// of the item; "Heals" alone when a formula decides it).
std::string staffEffectText(const Database& db, const ItemDef& item);

// "Hit 60%" / "Hit 60-80%" for the forecast: empty when the staff cannot miss.
std::string staffHitText(const StaffForecast& f);

}  // namespace ote
