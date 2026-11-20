// Bookkeeping of the art memory budget (docs/GRAPHICS.md "Art memory"): how many bytes each sheet holds on the
// graphics side, when it was last used, and which sheets to give back when the total is over the budget. No SDL,
// no textures: Sprites does the creating and destroying, this class only decides (so the rule is unit-tested).
//
// The rule: when over budget, the sheet used longest ago goes first, again and again until the total fits. A sheet
// used in the current frame (since the last beginFrame()) is never chosen, so a texture handed out in a frame stays
// valid until the frame ends; if everything is in use the total simply stays over budget until the next frame.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ote {

class ArtBudget {
 public:
  // `slots` sheets, none holding anything. A budget of 0 bytes means "no limit".
  void reset(size_t slots, uint64_t budgetBytes);
  // A new frame: what was used up to now may be given back when something else needs the room.
  void beginFrame() { ++frame_; }
  // The slot was just used (called on every lookup, so it stays cheap).
  void use(size_t slot) {
    lastFrame_[slot] = frame_;
    lastTick_[slot] = ++tick_;
  }
  // The slot now holds `bytes` (0 = nothing).
  void setBytes(size_t slot, uint64_t bytes);
  uint64_t bytes(size_t slot) const { return bytes_[slot]; }
  uint64_t total() const { return total_; }
  uint64_t budget() const { return budget_; }
  void setBudget(uint64_t budgetBytes) { budget_ = budgetBytes == 0 ? UINT64_MAX : budgetBytes; }
  size_t slots() const { return bytes_.size(); }

  // The slots to give back, least recently used first, until the total is within `limit` (default: the budget;
  // 0 = everything that is not in use this frame). Slots used this frame are skipped. The chosen slots are set to 0
  // bytes here; the caller frees what they held.
  std::vector<int> pickVictims() { return pickVictims(budget_); }
  std::vector<int> pickVictims(uint64_t limit);

  // Eviction report for the log, at most once per `everySec` seconds: records an eviction of `count` slots / `bytes`
  // at time `nowSec`, and returns true when it is time to log; then `count` / `bytes` hold everything since the
  // last report.
  bool noteEvicted(double nowSec, double everySec, uint64_t& count, uint64_t& bytes);

 private:
  uint64_t budget_ = UINT64_MAX, total_ = 0;
  uint64_t frame_ = 1;  // 1, not 0: before the first beginFrame() everything counts as in use (nothing is given back)
  uint64_t tick_ = 0;
  std::vector<uint64_t> bytes_, lastFrame_, lastTick_;
  uint64_t pendingCount_ = 0, pendingBytes_ = 0;
  double lastReport_ = -1e9;
};

}  // namespace ote
