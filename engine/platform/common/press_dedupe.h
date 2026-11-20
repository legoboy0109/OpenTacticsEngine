// Collapses one physical press that the platform delivers more than once into a single ButtonDown / ButtonUp. No SDL in
// here, so it is unit-tested (tests/test_press_dedupe.cpp). Why it exists (docs/XBOX.md "One B press"): on UWP / Xbox
// SDL's WinRT port sends the system back request as a key press of AC_BACK on top of the controller's own B button, and
// its Windows.Gaming.Input driver can list one pad several times, so each opened copy reports every button.
#pragma once
#include <cstdint>
#include <vector>

#include "platform/platform.h"

namespace ote {

// Where a press came from. Two presses are "the same source" only when kind and id both match: all keyboard keys are one
// source (a keyboard-only user pressing Escape and Backspace together gets exactly what they got before), the system
// back request is another, and each controller instance (SDL joystick instance id) is its own.
struct PressSource {
  enum Kind { Keyboard, BackRequest, Pad };
  Kind kind = Keyboard;
  int id = 0;
  bool operator==(const PressSource& o) const { return kind == o.kind && id == o.id; }
};

class PressDedupe {
 public:
  // The same button going down from a second source while the first still holds it, or within this long after the
  // first source's press, is a duplicate.
  static constexpr int64_t kWindowMs = 400;
  // A first source that has not been heard from for this long is no longer taken to hold the button (a lost release must
  // not block every other source for good). Keyboard auto-repeat of a direction refreshes it.
  static constexpr int64_t kHoldLimitMs = 3000;

  // True when the event should be passed on. A swallowed press also swallows that source's matching release.
  bool pass(Button button, PressSource src, bool down, int64_t nowMs);

  // Forget everything (a controller was removed: its releases may never come).
  void reset();

  // How many duplicates have been swallowed so far (the log line is written for the first one).
  int swallowed() const { return swallowed_; }

 private:
  struct State {
    bool known = false;
    PressSource owner;       // the source whose press was passed on last
    bool held = false;       // owner has gone down and not up
    int64_t lastMs = 0;      // when the owner last pressed (or repeated)
    std::vector<PressSource> muted;  // sources whose press was swallowed and whose release still has to be
  };
  static constexpr int kButtons = static_cast<int>(Button::Skip) + 1;
  State states_[kButtons];
  int swallowed_ = 0;
};

}  // namespace ote
