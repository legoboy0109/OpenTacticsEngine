#include "battle/art_budget.h"

namespace ote {

void ArtBudget::reset(size_t slots, uint64_t budgetBytes) {
  budget_ = budgetBytes == 0 ? UINT64_MAX : budgetBytes;
  total_ = 0;
  frame_ = 1;
  tick_ = 0;
  bytes_.assign(slots, 0);
  lastFrame_.assign(slots, 0);
  lastTick_.assign(slots, 0);
  pendingCount_ = pendingBytes_ = 0;
  lastReport_ = -1e9;
}

void ArtBudget::setBytes(size_t slot, uint64_t bytes) {
  total_ = total_ - bytes_[slot] + bytes;
  bytes_[slot] = bytes;
}

std::vector<int> ArtBudget::pickVictims(uint64_t limit) {
  std::vector<int> out;
  while (total_ > limit) {
    int best = -1;
    for (size_t i = 0; i < bytes_.size(); ++i) {
      if (bytes_[i] == 0 || lastFrame_[i] == frame_) continue;
      if (best < 0 || lastTick_[i] < lastTick_[(size_t)best]) best = (int)i;
    }
    if (best < 0) break;  // everything left is in use this frame
    out.push_back(best);
    setBytes((size_t)best, 0);
  }
  return out;
}

bool ArtBudget::noteEvicted(double nowSec, double everySec, uint64_t& count, uint64_t& bytes) {
  pendingCount_ += count;
  pendingBytes_ += bytes;
  if (nowSec - lastReport_ < everySec) return false;
  lastReport_ = nowSec;
  count = pendingCount_;
  bytes = pendingBytes_;
  pendingCount_ = pendingBytes_ = 0;
  return true;
}

}  // namespace ote
