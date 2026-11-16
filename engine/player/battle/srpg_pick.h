// The list behind the generic "pick a unit" / "pick a square" modes of the battle scene (battle_srpg_target.cpp):
// which candidate is highlighted and how the direction keys cycle it. Pure logic, no scene or drawing, so a test can
// drive it with fakes. The candidates are unit ids or squares (both are ints); the scene knows which.
#pragma once
#include <vector>

#include "platform/platform.h"

namespace ote {

class PickList {
 public:
  enum Result { None, Moved, Chosen, Cancelled };

  // Starts a pick; `preferred` (a candidate value, if present) is highlighted first, else the first.
  void begin(std::vector<int> candidates, int preferred = -1);
  bool empty() const { return items_.empty(); }
  int size() const { return (int)items_.size(); }
  int index() const { return idx_; }
  int current() const { return empty() ? -1 : items_[(size_t)idx_]; }
  const std::vector<int>& items() const { return items_; }
  // Right / Down / Next go on, Left / Up / Prev go back (wrapping); Confirm and Cancel end it; others do nothing.
  Result onButton(Button b);
  // A tap on candidate `value`: the first tap highlights it, a tap on the highlighted one chooses it.
  Result tap(int value);

 private:
  std::vector<int> items_;
  int idx_ = 0;
};

}  // namespace ote
