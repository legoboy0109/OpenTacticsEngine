// Words and small helpers about status conditions (docs/SRPG.md section 9 "Player"): the floating texts, the info panel rows, the
// staff forecast lines, why a unit cannot be given orders, and the short text tag drawn on the map (the engine has no small pictures
// for units, so a condition is shown by its "tag", a few letters). ALL placeholder wording of that part of the player lives here
// (the owner reviews it in one place). No scene, no drawing: unit-testable.
#pragma once
#include <string>
#include <vector>

#include "core/battle/battle.h"

namespace ote {

struct ConditionWords {
  static constexpr const char* kPage = "Status";                   // the unit details page
  static constexpr const char* kImmune = "Immune";                 // floats over a unit that resisted a condition
  static constexpr const char* kUntilCured = "Until cured";       // unit details
  static constexpr const char* kUntilMapEnd = "Until the map ends";
  static constexpr const char* kUntilCuredShort = "cure";          // the narrow side panel
  static constexpr const char* kUntilMapEndShort = "map";
  static constexpr const char* kLastTurn = "last turn";            // a count that reached 0: it ends when this turn does
  static constexpr const char* kLastTurnShort = "ends";
  static constexpr const char* kTurn = "turn";                     // "1 turn left"
  static constexpr const char* kTurns = "turns";
  static constexpr const char* kCannotAct = "cannot act";          // "Sleep: cannot act" (the hint line when such a unit is tapped)
  static constexpr const char* kActsAlone = "acts on its own";     // "Berserk: acts on its own"
  static constexpr const char* kCannotMove = "cannot move";
  static constexpr const char* kCures = "Cures";                   // forecast: "Cures Poison"
  static constexpr const char* kAlreadyAffected = "Already affected";  // a tapped unit that has it already
  static constexpr const char* kNothingToCure = "Nothing to cure";
  static constexpr const char* kMore = " more";
};

// The condition's name ("condition" for an index that is not one).
std::string conditionName(const Database& db, int condition);
// The short map tag of a condition ("Po"), and the tags a unit wears now in the order applied.
std::string conditionTag(const Database& db, int condition);
std::vector<std::string> conditionTags(const Battle& b, const Unit& u);

// How long it lasts, for the unit details ("3 turns left", "Until cured") and for the narrow side panel ("3", "cure", "map").
std::string conditionDurationText(const Database& db, const ConditionState& cs);
std::string conditionDurationShort(const Database& db, const ConditionState& cs);

// The floating texts of the events: "Poison!", "Cured Poison", "Poison ended", "Sleep" (the turn is skipped), "Berserk!" (it acts by itself).
std::string conditionAppliedText(const Database& db, int condition);
std::string conditionCuredText(const Database& db, int condition);
// "Sleep missed": an ability's chance to put the condition on failed (docs/SRPG.md "Ability conditions").
std::string conditionMissedText(const Database& db, int condition);
// "Sleep 60%": the chance an ability's condition takes hold on the unit under the cursor ("" for -1 = it inflicts nothing).
std::string conditionChanceText(const Database& db, int condition, int chance);
std::string conditionEndedText(const Database& db, int condition, int why);
std::string conditionTickText(const Database& db, int condition, int what);

// "Sleep: cannot act" / "Berserk: acts on its own" for a unit of the active side the player cannot give orders ("" = it can).
std::string unitStoppedText(const Battle& b, const Unit& u);
// "Stun: cannot move" when a condition stops it moving ("" = it can move).
std::string unitStuckText(const Battle& b, const Unit& u);

// The staff forecast lines about one unit that is hit: what it would be afflicted with ("Poison 3 turns") and what it would be cured of
// ("Cures Poison, Curse").
std::vector<std::string> staffConditionLines(const Battle& b, const StaffHit& h);

}  // namespace ote
