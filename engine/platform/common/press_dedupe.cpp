#include "platform/common/press_dedupe.h"

#include <algorithm>

#include "core/base/log.h"

namespace ote {

bool PressDedupe::pass(Button button, PressSource src, bool down, int64_t nowMs) {
  const int idx = static_cast<int>(button);
  if (idx <= 0 || idx >= kButtons) return true;  // Button::None and anything unknown: not ours to judge
  State& s = states_[idx];

  if (!down) {
    auto it = std::find(s.muted.begin(), s.muted.end(), src);
    if (it != s.muted.end()) {  // the release of a swallowed press
      s.muted.erase(it);
      return false;
    }
    if (s.known && s.owner == src) s.held = false;
    return true;
  }

  if (!s.known || s.owner == src) {  // first press ever, or the same source again (also its auto-repeat)
    s.known = true;
    s.owner = src;
    s.held = true;
    s.lastMs = nowMs;
    return true;
  }
  // A different source. A duplicate if the owner still holds the button (and has not gone silent for ages) or pressed
  // it a moment ago.
  const int64_t age = nowMs - s.lastMs;
  const bool duplicate = (s.held && age < kHoldLimitMs) || (age >= 0 && age < kWindowMs);
  if (!duplicate) {
    s.owner = src;
    s.held = true;
    s.lastMs = nowMs;
    s.muted.clear();
    return true;
  }
  if (std::find(s.muted.begin(), s.muted.end(), src) == s.muted.end()) s.muted.push_back(src);
  if (++swallowed_ == 1) {
    OTE_INFO("input: one press arrived from two sources (button %d); the second one is ignored (expected on Xbox: B "
             "is both a gamepad button and the system back request)", idx);
  }
  return false;
}

void PressDedupe::reset() {
  for (State& s : states_) s = State();
}

}  // namespace ote
