// The player's "danger zone" display (docs/SRPG.md "Enemy ranges"): the squares the enemies it marked (or all it can see) could
// move to next turn and, around those, hit. A display aid only: it reads the Battle, never changes it, is not saved and takes no part
// in a replay, the simulation or the computer opponent.
//
// What it shows is computed by core/battle/threat_area.h (the same reach and weapon-range rules as the player's own blue / red squares)
// as the viewer's team can know it: only enemies it sees at this moment, and nothing it cannot see shapes an area (a hidden unit is
// not a wall and not a gap; a door opened out of sight is still a door). The areas are cached: recomputed once per board change
// (Battle::stamp), never per frame.
#pragma once
#include <cstdint>
#include <vector>

#include "core/battle/battle.h"
#include "core/battle/threat_area.h"

namespace ote {

class EnemyRanges {
 public:
  // What to draw: 0 off (marks are refused), 1 movement + attack (two tints), 2 attack only (one tint over the whole danger area).
  void setStyle(int style);
  int style() const { return style_; }
  bool enabled() const { return style_ > 0; }

  // Is `unit` an enemy of `viewTeam` that the team sees now (the only kind that can be marked or counted in "all")?
  static bool markable(const Battle& b, int viewTeam, int unit);

  // Marks the enemy, or unmarks it when it is already marked. False (nothing changed) when it cannot be marked. `marked` is the new state.
  bool toggleMark(const Battle& b, int viewTeam, int unit, bool& marked);
  bool marked(int unit) const;
  // "Show all": every enemy seen.
  void toggleAll();
  bool all() const { return all_; }
  void clearMarks();

  // Brings the masks up to date for the board as it is now (cheap when nothing changed since the last call). Marks of units that
  // died or left are dropped; marks of units that are hidden are kept but not drawn. The viewer changing clears the marks.
  void update(const Battle& b, int viewTeam);
  // The union masks (one entry per cell) or null when nothing is shown. Valid until the next update().
  const std::vector<uint8_t>* moveMask() const { return shown_ ? &move_ : nullptr; }
  const std::vector<uint8_t>* attackMask() const { return shown_ ? &attack_ : nullptr; }
  // What the last update worked out (for the log line and the tests): units drawn, squares in the move tint, squares in the attack-only tint.
  int unitCount() const { return units_; }
  int moveSquares() const { return moveSquares_; }
  int attackSquares() const { return attackSquares_; }
  // How many whole recomputations ran (the tests check that a frame without a board change costs none).
  int rebuilds() const { return rebuilds_; }

 private:
  int style_ = 1;
  bool all_ = false;
  std::vector<int> marks_;  // unit ids, in the order marked
  unsigned marksRev_ = 0;   // changes whenever marks or "all" change
  int team_ = -2;
  // What the masks were built from.
  bool built_ = false;
  uint64_t builtStamp_ = 0;
  int builtTeam_ = -2, builtStyle_ = -1;
  unsigned builtRev_ = 0;
  // One area per unit, valid for cacheStamp_ (so marking another unit does not redo the others).
  std::vector<ThreatArea> cache_;
  std::vector<char> have_;
  uint64_t cacheStamp_ = 0;
  size_t cacheCells_ = 0;
  Reach scratch_;
  std::vector<uint8_t> move_, attack_;
  bool shown_ = false;
  int units_ = 0, moveSquares_ = 0, attackSquares_ = 0, rebuilds_ = 0;
};

}  // namespace ote
