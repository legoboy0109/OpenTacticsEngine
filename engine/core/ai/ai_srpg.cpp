// The computer opponent and the strategy-RPG features (docs/SRPG.md "AI (minimum behaviour)").
//
// What the AI does with each feature, in one place:
//   promotion  - takes the first offered one when its turn would otherwise be a plain Wait or advance (here);
//   abilities  - summon and heal only (ai_abilities.cpp); push, pull, swap, moveBehind are never used;
//   move again - a step away from danger with what is left of the move (ai_again.cpp);
//   act again  - nothing to do: the unit's flags are cleared, so the next call simply plans it afresh (once per turn
//                by the skill's own rule, so no loop);
//   rescue, give, take, pair up, switch, drop, supports - never, so a unit of an AI side is never carried, and
//                (R1's warning) Give / Take, which could repeat without limit, cannot loop.
#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

namespace {

// A plain Wait or advance (no attack, capture, item or supply) is the only order a promotion may replace.
bool promotionOnOffer(const Battle& b, const Unit& u, Order& order) {
  if (order.kind != ActionType::Wait) return false;
  std::vector<int> options;
  b.promotionOptions(u.id, -1, options);
  if (options.empty()) return false;
  // `validate` is the last word (a unit that already moved, a type that cannot stand here, ...).
  if (!b.validate(Action::promote(u.id), nullptr)) return false;
  // In place and with the order's score, so the unit keeps its place in the queue. Moving first would put the
  // new type on a square promotionOptions did not check.
  order.kind = ActionType::Promote;
  order.moveTo = kNoCell;
  return true;
}

}  // namespace

void considerSrpg(const Battle& b, const Unit& u, Order& order) {
  if (promotionOnOffer(b, u, order)) return;
  considerAbilities(b, u, order);
}

}  // namespace ai
}  // namespace ote
