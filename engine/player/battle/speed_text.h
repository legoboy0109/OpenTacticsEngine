// Words of the speed turn order (docs/TURN_ORDER.md "Player") for the turn strip, the floating notes and the log. ALL placeholder wording of that
// part of the player lives here. No scene, no drawing.
#pragma once
#include <string>

#include "core/battle/battle.h"

namespace ote {

struct SpeedWords {
  static constexpr const char* kCharging = "Charging ";        // over the caster: "Charging Fire"
  static constexpr const char* kGoesOff = "";                  // over the square: "Fire!"
  static constexpr const char* kHidden = "?";                  // a unit the viewer cannot see, in the strip
  static constexpr const char* kCancelled[5] = {"Charge lost", "Interrupted", "Charge given up", "Fizzled", "Charge lost"};  // by kCharge* reason
};

// The name of the ability or weapon a charge event or pending action is about.
inline std::string chargeActionName(const Battle& b, bool weapon, int index) {
  if (weapon) return index >= 0 && (size_t)index < b.db->weapons.size() ? b.db->weapons[(size_t)index].name : "?";
  return index >= 0 && (size_t)index < b.db->abilities.size() ? b.db->abilities[(size_t)index].name : "?";
}

}  // namespace ote
