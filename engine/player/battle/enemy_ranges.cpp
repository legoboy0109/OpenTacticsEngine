#include "battle/enemy_ranges.h"

#include <algorithm>
#include <chrono>

#include "core/base/log.h"

namespace ote {

void EnemyRanges::setStyle(int style) {
  style_ = style < 0 ? 0 : style > 2 ? 2 : style;
  if (style_ == 0) clearMarks();
}

bool EnemyRanges::markable(const Battle& b, int viewTeam, int unit) {
  if (unit < 0 || unit >= (int)b.units.size()) return false;
  const Unit& u = b.units[(size_t)unit];
  return u.alive && u.cell != kNoCell && b.teamOf(u) != viewTeam && b.unitVisibleTo(viewTeam, u);
}

bool EnemyRanges::marked(int unit) const { return std::find(marks_.begin(), marks_.end(), unit) != marks_.end(); }

bool EnemyRanges::toggleMark(const Battle& b, int viewTeam, int unit, bool& isMarked) {
  if (!enabled() || !markable(b, viewTeam, unit)) return false;
  auto it = std::find(marks_.begin(), marks_.end(), unit);
  if (it != marks_.end()) {
    marks_.erase(it);
    isMarked = false;
  } else {
    marks_.push_back(unit);
    isMarked = true;
  }
  ++marksRev_;
  return true;
}

void EnemyRanges::toggleAll() {
  if (!enabled()) return;
  all_ = !all_;
  ++marksRev_;
}

void EnemyRanges::clearMarks() {
  if (!marks_.empty() || all_) ++marksRev_;
  marks_.clear();
  all_ = false;
}

void EnemyRanges::update(const Battle& b, int viewTeam) {
  if (viewTeam != team_) {  // another viewer (hot-seat): what one player marked is not for the next (the first viewer keeps its marks)
    if (team_ != -2) clearMarks();
    team_ = viewTeam;
  }
  // Marks of units that are gone (defeated, carried off) are dropped for good; a hidden one is kept, just not drawn.
  const size_t before = marks_.size();
  marks_.erase(std::remove_if(marks_.begin(), marks_.end(),
                              [&b](int id) { return id >= (int)b.units.size() || !b.units[(size_t)id].alive || b.units[(size_t)id].cell == kNoCell; }),
               marks_.end());
  if (marks_.size() != before) ++marksRev_;

  if (built_ && builtStamp_ == b.stamp() && builtTeam_ == viewTeam && builtStyle_ == style_ && builtRev_ == marksRev_) return;
  const bool wasShown = shown_;
  using Clock = std::chrono::steady_clock;
  const Clock::time_point t0 = Clock::now();

  const size_t cells = (size_t)b.grid.cellCount();
  if (cacheStamp_ != b.stamp() || have_.size() != b.units.size() || cacheCells_ != cells) {
    cacheCells_ = cells;
    cacheStamp_ = b.stamp();
    have_.assign(b.units.size(), 0);
    cache_.resize(b.units.size());
  }
  move_.assign(cells, 0);
  attack_.assign(cells, 0);
  units_ = 0;
  auto include = [&](int id) {
    if (!markable(b, viewTeam, id)) return;
    if (!have_[(size_t)id]) {
      threatArea(b, id, viewTeam, scratch_, cache_[(size_t)id]);
      have_[(size_t)id] = 1;
    }
    const ThreatArea& a = cache_[(size_t)id];
    for (size_t c = 0; c < cells; ++c) {
      move_[c] |= a.move[c];
      attack_[c] |= a.attack[c];
    }
    ++units_;
  };
  if (enabled()) {
    if (all_) {
      for (const Unit& u : b.units) include(u.id);
    } else {
      for (int id : marks_) include(id);
    }
  }
  moveSquares_ = attackSquares_ = 0;
  if (style_ == 2) {  // one tint over everything an enemy can reach or hit
    for (size_t c = 0; c < cells; ++c) {
      attack_[c] |= move_[c];
      move_[c] = 0;
      if (attack_[c]) ++attackSquares_;
    }
  } else {
    for (size_t c = 0; c < cells; ++c) {
      if (move_[c]) ++moveSquares_;
      else if (attack_[c]) ++attackSquares_;
    }
  }
  shown_ = units_ > 0;
  built_ = true;
  builtStamp_ = b.stamp();
  builtTeam_ = viewTeam;
  builtStyle_ = style_;
  builtRev_ = marksRev_;
  ++rebuilds_;
  if (shown_ || wasShown) {
    const long us = (long)std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count();
    OTE_INFO("enemy ranges: %d units, %d move squares, %d attack squares", units_, moveSquares_, attackSquares_);
    OTE_DEBUG("enemy ranges: computed in %ld us", us);
  }
}

}  // namespace ote
