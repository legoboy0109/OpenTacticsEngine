#include "battle/unit_info_rank.h"

#include "ui/text.h"

namespace ote {

std::string unitRankName(const Battle& b, const Unit& u) {
  const int rank = b.unitRank(u);
  return rank < 0 ? std::string("No rank") : b.db->rules.unitRanks.levels[(size_t)rank].name;
}

std::string unitRankXpText(const Battle& b, const Unit& u, bool label) {
  const UnitRankRules& t = b.db->rules.unitRanks;
  const int rank = b.unitRank(u);
  const std::string head = label ? "XP " : "";
  if (rank + 1 >= (int)t.levels.size()) return head + intStr(u.xp) + " MAX";
  return head + intStr(u.xp) + "/" + intStr(t.levels[(size_t)rank + 1].at);
}

}  // namespace ote
