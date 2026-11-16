// Weapon ranks and stat caps: the data (contract: docs/SRPG.md section 7). Included by database.h.
#pragma once
#include <string>
#include <vector>

namespace ote {

struct RankDef {
  std::string id, name;
  int xp = 0;  // weapon experience needed to reach it; the first rank is 0
};

const int kMaxRanks = 12;
const int kMaxRankXp = 9999;
const int kMaxRankUseXp = 99;  // a weapon's "rankXp"
const int kMaxStatCap = 99999;

// data/ranks.json. `ladder` empty = the game has no weapon ranks (every rank feature is off).
struct RankRules {
  std::vector<RankDef> ladder;
  bool growByUse = false;              // "growth": "use"; else the ranks are fixed (class, character, promotion)
  std::vector<std::string> groups;     // the weapon groups that exist, in weapons.json order; Unit::weaponXp is indexed by it

  bool on() const { return !ladder.empty(); }
  int topXp() const { return ladder.empty() ? 0 : ladder.back().xp; }
  // Index of the highest ladder entry whose xp is not above `xp`.
  int rankAt(int xp) const {
    int r = 0;
    for (size_t i = 0; i < ladder.size(); ++i)
      if (ladder[i].xp <= xp) r = (int)i;
    return r;
  }
  int findRank(const std::string& id) const {
    for (size_t i = 0; i < ladder.size(); ++i)
      if (ladder[i].id == id) return (int)i;
    return -1;
  }
  int findGroup(const std::string& id) const {
    for (size_t i = 0; i < groups.size(); ++i)
      if (groups[i] == id) return (int)i;
    return -1;
  }
};

}  // namespace ote
