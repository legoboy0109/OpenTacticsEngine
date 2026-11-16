#include "battle/srpg_pick.h"

#include <algorithm>

namespace ote {

void PickList::begin(std::vector<int> candidates, int preferred) {
  items_ = std::move(candidates);
  idx_ = 0;
  auto it = std::find(items_.begin(), items_.end(), preferred);
  if (it != items_.end()) idx_ = (int)(it - items_.begin());
}

PickList::Result PickList::onButton(Button b) {
  int n = size();
  switch (b) {
    case Button::Right: case Button::Down: case Button::Next:
      if (n == 0) return None;
      idx_ = (idx_ + 1) % n;
      return Moved;
    case Button::Left: case Button::Up: case Button::Prev:
      if (n == 0) return None;
      idx_ = (idx_ + n - 1) % n;
      return Moved;
    case Button::Confirm: return n > 0 ? Chosen : None;
    case Button::Cancel: return Cancelled;
    default: return None;
  }
}

PickList::Result PickList::tap(int value) {
  auto it = std::find(items_.begin(), items_.end(), value);
  if (it == items_.end()) return None;
  int i = (int)(it - items_.begin());
  if (i == idx_) return Chosen;
  idx_ = i;
  return Moved;
}

}  // namespace ote
