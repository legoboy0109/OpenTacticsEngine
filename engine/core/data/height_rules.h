// Height rules (docs/SRPG.md "Height rules"): game.json "rules": { "height": ... }. When the block is present the per-cell heights of the
// maps (they were always drawn, and formulas read them as `ha` / `hb` / `dh`) also limit steps and shots. A game without the block is
// unchanged. Rules live in core/battle/height_rules.cpp; the loader is core/data/loader_height.cpp.
#pragma once
#include <vector>

namespace ote {

// "No limit" for a jump / a reach in levels: larger than any height difference a map can have.
constexpr int kNoHeightLimit = 1000000;

struct HeightRules {
  bool on = false;
  // "jump": { "default": n } or { "default": { "up": n, "down": m } }: the most a unit may climb / drop in one step; a unit type may say its own.
  int jumpUp = kNoHeightLimit, jumpDown = kNoHeightLimit;
  std::vector<int> fliers;  // "flyingMoveTypes": move types (indices into Database::moveTypes) that ignore the jump rule
  // "rangeBonus": { "perLevels": k, "max": b }: +1 maximum range per k levels the shooter stands above the target, at most b (ranged weapons only).
  int rangeEvery = 0, rangeMax = 0;  // rangeEvery 0 = no bonus
  bool lineOfSight = false;          // "lineOfSight": a "fire": "line" shot is also stopped by a square higher than both ends
  bool flies(int moveType) const {
    for (int m : fliers)
      if (m == moveType) return true;
    return false;
  }
};

// A weapon's "heightRange": { "up": n, "down": m }: the target may stand at most n levels higher / m levels lower than the shooter. -1 = no limit.
struct HeightRange {
  int up = -1, down = -1;
  bool any() const { return up >= 0 || down >= 0; }
};

}  // namespace ote
