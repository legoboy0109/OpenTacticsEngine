// Difficulties (docs/DIFFICULTY.md): the maker's own list of how hard the game can be, chosen at New Game. A game
// without game.json "difficulties" has none of this anywhere (Database::difficulties.list is empty).
#pragma once
#include <string>
#include <vector>

namespace ote {

// How the enemies of the human sides are changed. Used for the game's numbers and for a map's override of them.
struct DifficultyEnemy {
  int levels = 0;                // built as if placed this many levels higher (negative: lower), never below level 1
  std::vector<int> stats;        // per stat index: flat addition after the level shift; empty = none
  int statsPercent = 100;        // scales the stats of DifficultyRules::percentStatIndex, after the flat additions
  // What a map's override states (an override replaces only what it names; the game's numbers fill the rest).
  bool hasLevels = false, hasPercent = false;
  std::vector<char> hasStat;     // per stat index: the override names it (`stats` then holds its value)
  bool any() const {
    if (levels != 0 || statsPercent != 100) return true;
    for (int s : stats)
      if (s != 0) return true;
    return false;
  }
};

struct DifficultyDef {
  std::string id, name, description;
  DifficultyEnemy enemy;
  int xpPercent = 100;        // experience gained by units of human sides
  int resourcePercent = 100;  // resources gained by human sides (income, giveResource, loot)
};

// game.json "difficulties" "change": when the player may change the difficulty after New Game.
enum class DifficultyChange { Never, LowerOnly, Free };

struct DifficultyRules {
  std::string defaultId;                    // the one preselected / used when nothing was chosen
  int defaultIndex = 0;
  DifficultyChange change = DifficultyChange::LowerOnly;
  std::vector<int> percentStatIndex;        // the stats "statsPercent" scales (every stat but the movement one unless "percentStats" says)
  bool percentStatsGiven = false;           // the maker listed them ("percentStats")
  std::vector<DifficultyDef> list;          // easiest to hardest; empty = the game has no difficulties

  bool any() const { return !list.empty(); }
  int find(const std::string& id) const {
    for (size_t i = 0; i < list.size(); ++i)
      if (list[i].id == id) return (int)i;
    return -1;
  }
  // May the player go from `from` to `to` now? (never: no; lowerOnly: only to an easier one; free: always)
  bool mayChange(int from, int to) const {
    if (to < 0 || to >= (int)list.size() || from == to) return false;
    switch (change) {
      case DifficultyChange::Never: return false;
      case DifficultyChange::LowerOnly: return from < 0 || to < from;
      case DifficultyChange::Free: return true;
    }
    return false;
  }
};

// A map's own numbers for one difficulty ("difficulty": { "<id>": {...} } in a map file).
struct MapDifficulty {
  int difficulty = -1;  // index into DifficultyRules::list
  DifficultyEnemy enemy;
  int xpPercent = -1, resourcePercent = -1;  // -1 = the game's
};

}  // namespace ote
