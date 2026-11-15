// Words of the fog-of-war part of the player (docs/ECONOMY.md "Fog of war"). ALL placeholder wording of it lives
// here (like economy_text.*): nothing else spells these strings out.
#pragma once

namespace ote {

struct FogWords {
  // The cover between two human turns on one device.
  static constexpr const char* kPassTo = "Pass the device to ";
  static constexpr const char* kPassHint = "Confirm / tap: start the turn";
  // An ambush: the move was stopped by an enemy nobody had seen.
  static constexpr const char* kFloatAmbush = "Ambush!";
  static constexpr const char* kNoticeStopped = "Stopped: an enemy was in the way";
};

}  // namespace ote
