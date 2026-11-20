// Unit ranks (docs/SRPG.md "Unit ranks"): game.json "rules": { "progression": { "levels": [ ... ] } }. A table of named steps on a unit's
// total experience (a veteran, an elite): the unit's rank is the highest entry whose `at` its experience has reached, each rank may add stats
// and skills, and the player shows a small badge on the map. A game with the table does NOT level up (the experience is a running total that
// stops at the top rank's `at`); a game without it is unchanged. The rules live in core/battle/unit_ranks.cpp, the loader in loader_unit_ranks.cpp.
#pragma once
#include <string>
#include <vector>

namespace ote {

struct UnitRankDef {
  std::string name;
  std::string badge;        // the 1-3 characters drawn on the unit on the map; "" = none
  int at = 0;               // total experience that earns this rank
  std::vector<int> bonus;   // per stat index; empty = none (else sized to the stat list)
  std::vector<int> skills;  // indices into Database::skills the unit has while it holds this rank
};

struct UnitRankRules {
  std::vector<UnitRankDef> levels;                // rising `at`; empty = the game has no rank table
  std::vector<std::vector<std::string>> skillIds;  // loader scratch: the "skills" ids of each entry until the skills are known

  bool on() const { return !levels.empty(); }
  // Index of the highest entry whose `at` is not above `xp`; -1 = below the first one (no rank yet).
  int rankAt(int xp) const {
    int r = -1;
    for (size_t i = 0; i < levels.size(); ++i)
      if (levels[i].at <= xp) r = (int)i;
    return r;
  }
  int topAt() const { return levels.empty() ? 0 : levels.back().at; }
  // What the rank a unit with `xp` holds adds to stat `stat`.
  int bonusAt(int xp, int stat) const {
    const int r = rankAt(xp);
    if (r < 0 || levels[(size_t)r].bonus.empty() || (size_t)stat >= levels[(size_t)r].bonus.size()) return 0;
    return levels[(size_t)r].bonus[(size_t)stat];
  }
};

}  // namespace ote
