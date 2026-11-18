#include "difficulty_rows.h"

namespace ote {

std::vector<int> difficultiesReachable(const DifficultyRules& rules, int now) {
  std::vector<int> out;
  for (size_t i = 0; i < rules.list.size(); ++i)
    if (rules.mayChange(now, (int)i)) out.push_back((int)i);
  return out;
}

std::string difficultyBlockedNote(const DifficultyRules& rules, int now) {
  if (rules.change == DifficultyChange::Never) return " (fixed)";
  if (difficultiesReachable(rules, now).empty()) return rules.change == DifficultyChange::LowerOnly ? " (lowest)" : "";
  return "";
}

}  // namespace ote
