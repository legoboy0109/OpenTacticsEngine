#include "battle/unit_info_rank.h"

#include "ui/text.h"

namespace ote {

std::string unitRankName(const Battle& b, const Unit& u) {
  const int rank = b.unitRank(u);
  return rank < 0 ? std::string("No rank") : b.db->rules.unitRanks.levels[(size_t)rank].name;
}

std::string unitRankXpText(const Battle& b, const Unit& u, bool label, bool noMax) {
  const UnitRankRules& t = b.db->rules.unitRanks;
  const int rank = b.unitRank(u), points = b.unitRankPoints(u);
  // The counter's name: "XP" in the experience mode, the game's own word ("Merit") with "rules.ranks".
  const std::string head = label ? (t.merit ? t.counter : std::string("XP")) + " " : "";
  if (rank + 1 >= (int)t.levels.size()) return head + intStr(points) + (noMax ? "" : " MAX");
  return head + intStr(points) + "/" + intStr(t.levels[(size_t)rank + 1].at);
}

}  // namespace ote
