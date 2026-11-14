// The AI's one item rule (docs/ITEMS.md "AI"): a unit below half its hit points that carries a healing
// item for itself uses it instead of attacking on, unless its attack would kill. It does not trade, shop
// or use the storage.
#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

namespace {

// Slot of the first usable item that heals its carrier and would do something now (-1 = none).
int selfHealSlot(const Battle& b, const Unit& u) {
  std::vector<int> targets;
  for (size_t i = 0; i < u.items.size(); ++i) {
    const ItemDef& d = b.itemOf(u.items[i]);
    if (d.kind != ItemKind::Usable || d.targetAlly || d.heal <= 0) continue;
    b.useTargets(u.id, (int)i, targets);
    if (!targets.empty()) return (int)i;
  }
  return -1;
}

// True if the planned attack is expected to finish its target (counting every strike as a hit).
bool attackKills(const Battle& b, const Unit& u, const Order& o) {
  Cell from = o.moveTo == kNoCell ? u.cell : o.moveTo;
  Forecast f;
  if (!b.forecast(u.id, o.target, o.weapon, from, f)) return false;
  return f.attack.damage * f.attack.strikes >= b.units[(size_t)o.target].hp;
}

}  // namespace

void considerHealing(const Battle& b, const Unit& u, Order& order) {
  if (u.items.empty() || u.hp * 2 >= b.maxHp(u)) return;
  if (order.kind != ActionType::Attack && order.kind != ActionType::Wait) return;  // a capture goes on
  if (order.kind == ActionType::Attack && attackKills(b, u, order)) return;
  int slot = selfHealSlot(b, u);
  if (slot < 0) return;
  // Same square the AI chose (it moves first), the order's score stays: it keeps its place in the queue.
  order.kind = ActionType::UseItem;
  order.slot = slot;
  order.target = u.id;
  order.weapon = -1;
}

}  // namespace ai
}  // namespace ote
