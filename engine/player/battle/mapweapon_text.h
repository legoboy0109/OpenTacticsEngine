// Words about map weapons (a ballista's weapon a unit standing on the structure fires; docs/SRPG.md "Map weapons") for the forecast
// box, the info panel and the log. ALL placeholder wording of that part of the player lives here. No scene, no drawing.
#pragma once
#include <string>

#include "core/battle/battle.h"

namespace ote {

struct MapWeaponWords {
  static constexpr const char* kRow = "Weapon";        // terrain info row: "Weapon   Bolt 2/3"
  static constexpr const char* kFrom = "From ";        // forecast box, under the weapon: "From Ballista"
  static constexpr const char* kSpent = "spent";       // "Bolt spent"
  static constexpr const char* kFloatSpent = " spent"; // "Ballista spent" over the structure after its last shot
};

// "2/3" (uses left of the most it had), "" when it never runs out, "spent" at 0.
std::string mapWeaponUsesText(const Battle& b, int structure);
// The terrain info row's value: "Bolt 2/3", "Bolt" (unlimited), "Bolt spent". Empty when the structure has no weapon now.
std::string mapWeaponRowValue(const Battle& b, int structure);
// The forecast box's weapon line for the weapon `u` fires from the structure: "Bolt 2" (uses left; just "Bolt" when unlimited), and
// under it the line "From Ballista" (two short lines fit a phone's narrow box where one long one would be cut).
std::string mapWeaponForecastName(const Battle& b, int structure);
std::string mapWeaponForecastFrom(const Battle& b, int structure);
// The unit panel's row for a unit standing on a structure with a weapon it may use: label = weapon name, value = "2/3 r2-6".
// False when the square offers it nothing (or the unit already carries that weapon).
bool mapWeaponUnitRow(const Battle& b, const Unit& u, std::string& label, std::string& value);
// The floating note over the unit after the structure's last shot: "Ballista spent".
std::string mapWeaponSpentFloat(const Battle& b, const Event& attack);
// The one log line of a strike made with a structure's weapon ("map weapon: Mara fires Bolt from Ballista (spent)").
std::string mapWeaponLogLine(const Battle& b, const Event& attack);

}  // namespace ote
