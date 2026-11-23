// Words about menu abilities for the action menu, the targeting hint, the Abilities page of Unit details and the
// battle's floating texts (docs/SRPG.md step U3). ALL placeholder wording of that part of the player lives in
// AbilityWords and unit_info_abilities_text.cpp (the owner reviews it in one place). No scene, no drawing:
// unit-testable.
#pragma once
#include <string>
#include <vector>

#include "core/battle/battle.h"

namespace ote {

struct AbilityWords {
  static constexpr const char* kLeft = " left";  // menu: "2 left" (uses left this battle)
  static constexpr const char* kNothingToTarget = "Nothing to target";
  static constexpr const char* kNone = "No abilities.";
  static constexpr const char* kBlocked = "Blocked!";
  static constexpr const char* kMore = " more...";  // Abilities page: "+2 more..." when the list does not fit
  // Steal (docs/SRPG.md "Steal"): the list of the foe's items, the floating texts of the events.
  static constexpr const char* kStealWhich = "Steal which?";
  static constexpr const char* kStole = "Stole ";    // "Stole Potion"
  static constexpr const char* kAgain = "Ready again";  // over a unit that was refreshed
};
// The plain reason an item cannot be stolen (the core's text), in the words of the list: the long form, or the short one for a narrow menu.
std::string stealReasonText(const std::string& why, bool shortForm);

// What the effect does, in one phrase: "Pushes it 2 squares away", "Heals 10 HP", "Heals all HP", "Pulls it next to
// you", "Swaps places with it", "Moves it behind you", "Calls a Hound (level 3, up to 2 at a time, lasts 3 turns)".
// withChance = false leaves out an inflict's chance (the pick hint shows the chance against the highlighted target instead).
std::string abilityEffectText(const Database& db, const AbilityDef& def, bool withChance = true);
// Who or what it is used on: "yourself", "an ally", "an enemy", "any unit", "a square".
std::string abilityTargetText(const AbilityDef& def);
// "range 1", "range 1-2"; empty for a "self" ability.
std::string abilityRangeText(const AbilityDef& def);
// "ends the turn" or "free".
std::string abilityCostText(const AbilityDef& def);
// The limits of the ability itself: "2 uses per battle, ready again after 3 turns, only before moving" (may be empty).
std::string abilityLimitText(const AbilityDef& def);
// What the unit has left of it right now: "1 of 2 uses left, ready in 2 turns", "used this turn" (empty when nothing
// is limited or used up).
std::string abilityStatusText(const Battle& b, const Unit& u, int ability);

// The menu's right-hand text of one entry: the plain reason when it cannot be used, else its price ("6 MP") and the uses left ("2 left")
// for an ability with a limit, else empty.
std::string abilityMenuExtra(const Battle& b, const Unit& u, const AbilityOption& o);
// What one use costs from "spend": "6 MP", "6 MP 10 Gold"; empty for an ability without a price (docs/SRPG.md "Unit resources").
std::string abilityPriceText(const Database& db, const AbilityDef& def);
// A shorter form of one of the core's reasons, for a menu with no room for the full one ("before moving" for "can only
// be used before moving"); a text that is not one of them comes back as it was.
std::string abilityShortReason(const std::string& reason);

}  // namespace ote
