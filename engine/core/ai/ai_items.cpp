// The AI's one item rule (docs/ITEMS.md "AI"): a unit below half its hit points that carries a healing
// item for itself uses it instead of attacking on, unless its attack would kill. It does not trade, shop
// or use the storage.
#include <algorithm>

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
  return f.attack.totalDamage() >= b.units[(size_t)o.target].hp;
}

// Slot of a cure item (usable, "cure" / "cure all") the unit carries that has a unit to cure now, and that unit: itself first, else a unit next to
// it when `staysPut` (the item's own targets, unit-id order). -1 = none. What an item cures is the maker's own list (as for staves, no judging of curses).
int cureSlot(const Battle& b, const Unit& u, bool staysPut, int& target) {
  std::vector<int> targets;
  for (size_t i = 0; i < u.items.size(); ++i) {
    const ItemDef& d = b.itemOf(u.items[i]);
    if (d.kind != ItemKind::Usable || (d.cures.empty() && !d.cureAll)) continue;
    b.useTargets(u.id, (int)i, targets);
    int pick = -1;
    for (int id : targets) {
      const Unit& t = b.units[(size_t)id];
      bool sick = false;
      for (const ConditionState& cs : t.conditions)
        sick = sick || d.cureAll || std::find(d.cures.begin(), d.cures.end(), cs.condition) != d.cures.end();
      if (sick && (id == u.id || staysPut) && (pick < 0 || id == u.id)) pick = id;
    }
    if (pick >= 0) {
      target = pick;
      return (int)i;
    }
  }
  return -1;
}

}  // namespace

void considerCuring(const Battle& b, const Unit& u, Order& order) {
  if (u.items.empty() || (order.kind != ActionType::Attack && order.kind != ActionType::Wait)) return;
  if (order.kind == ActionType::Attack && attackKills(b, u, order)) return;
  int target = -1;
  const int slot = cureSlot(b, u, order.moveTo == kNoCell, target);
  if (slot < 0) return;
  order.kind = ActionType::UseItem;
  order.slot = slot;
  order.target = target;
  order.weapon = -1;
}

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
