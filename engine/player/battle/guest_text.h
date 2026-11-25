// The player's words for guests (docs/SRPG.md "Guests"). Placeholder wording: the owner may change any of it.
#pragma once

namespace ote {

struct GuestWords {
  static constexpr const char* kGuest = "Guest";          // the unit panel's row label, and "Guest Ritz is acting..." on the hint line
  static constexpr const char* kMovedBy = "moves itself";  // the panel row's value: the computer moves it
};

}  // namespace ote
