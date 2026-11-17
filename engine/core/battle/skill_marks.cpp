// Marks of a fight (docs/SRPG.md "Linking two strike blocks"): the effect `{ "mark": "name" }` of a strike / struck block that takes
// effect, and the condition `{ "marked": "name" }` of the same unit's other blocks in the same fight. Nothing here is saved: a mark
// is set and read inside one attack action and cleared when it is over (combat.cpp), so no save ever holds one.
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

bool Battle::markSet(int unit, const std::string& name) const {
  for (const std::pair<int, std::string>& m : fightMarks_)
    if (m.first == unit && m.second == name) return true;
  return false;
}

void Battle::setMark(int unit, const std::string& name) const {
  if (!markSet(unit, name)) fightMarks_.emplace_back(unit, name);
}

bool Battle::blockReadsMarks(const SkillBlock& b) {
  for (const SkillCond& c : b.conds)
    if (c.kind == SkillCond::Kind::Marked) return true;
  return false;
}

}  // namespace ote
