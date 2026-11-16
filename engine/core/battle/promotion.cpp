// Promotion in a battle (docs/SRPG.md "Promotion").
// STUB (step F1): no unit type has promotions yet (the loader refuses the key), so nothing can promote.
// Step P1 fills this file.
#include "core/battle/battle.h"

namespace ote {

void Battle::promotionOptions(int, int, std::vector<int>& out) const { out.clear(); }

bool Battle::promote(int, int, const PromotionDef*, std::vector<Event>&) { return false; }

bool Battle::validatePromote(const Action&, std::string* why) const {
  if (why) *why = "this game has no promotions";
  return false;
}

void Battle::applyPromote(const Action&, std::vector<Event>&) {}
void Battle::promotionLevelUp(int, std::vector<Event>&) {}
void Battle::promotionTurnStart(std::vector<Event>&) {}
void Battle::storyPromote(const Command&, std::vector<Event>&) {}

}  // namespace ote
