// Menu abilities (docs/SRPG.md "AI"): the AI only summons and heals. Push, pull, swap and moveBehind are never used.
// A free ability is used at most once per unit per turn by the ability rule itself, so the stateless planner (it plans
// every unit afresh at each call) cannot loop on it.
#include <algorithm>

#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

namespace {

// The legal square nearest to the nearest known enemy (lowest cell number on a tie; with no enemy known, the lowest).
Cell summonSquare(const Battle& b, const Unit& u, const std::vector<Cell>& cells) {
  Cell best = kNoCell;
  int bestDist = kFar;
  for (Cell c : cells) {
    int d = kFar;
    for (const Unit& e : b.units)
      if (e.alive && b.isEnemy(u, e) && knownEnemy(b, u.side, e)) d = std::min(d, b.grid.distance(c, e.cell));
    if (best == kNoCell || d < bestDist || (d == bestDist && c < best)) {
      best = c;
      bestDist = d;
    }
  }
  return best;
}

// The unit (itself or an adjacent ally) below half HP with the lowest share of its HP left; -1 = nobody.
int healTarget(const Battle& b, const Unit& u, const std::vector<int>& targets) {
  int best = -1;
  double bestShare = 1;
  for (int id : targets) {
    const Unit& t = b.units[(size_t)id];
    if (!t.alive || b.isEnemy(u, t) || t.hp * 2 >= b.maxHp(t)) continue;
    if (id != u.id && b.grid.distance(u.cell, t.cell) > 1) continue;  // "neighbouring"
    double share = t.hp / (double)std::max(1, b.maxHp(t));
    if (best < 0 || share < bestShare || (share == bestShare && id < best)) {
      best = id;
      bestShare = share;
    }
  }
  return best;
}

bool plannedKill(const Battle& b, const Unit& u, const Order& o) {
  if (o.kind != ActionType::Attack) return false;
  Forecast f;
  Cell from = o.moveTo == kNoCell ? u.cell : o.moveTo;
  return b.forecast(u.id, o.target, o.weapon, from, f) &&
         f.attack.damage * f.attack.strikes >= b.units[(size_t)o.target].hp;
}

// Tries every ability of the unit as if it stood on `from`; fills `order` with the first summon or heal that works.
bool tryFrom(const Battle& b, const Unit& u, Cell from, Order& order, bool wantSummon) {
  std::vector<AbilityOption> menu;
  b.abilityOptions(u.id, from, menu);
  for (const AbilityOption& o : menu) {
    if (!o.ok) continue;
    const AbilityDef& d = b.db->abilities[(size_t)o.ability];
    // A free ability is used on the spot, before any move, and the unit is then planned afresh with its turn open.
    if (d.free && from != u.cell) continue;
    const bool summon = d.effect == AbilityEffect::Summon;
    if (summon != wantSummon || (!summon && d.effect != AbilityEffect::Heal)) continue;
    std::vector<int> units;
    std::vector<Cell> cells;
    b.abilityTargets(u.id, o.ability, from, units, cells);
    Order n;
    n.score = order.score;
    n.kind = ActionType::Ability;
    n.ability = o.ability;
    n.moveTo = (from == u.cell || d.free) ? kNoCell : from;
    if (summon) {
      n.cell = summonSquare(b, u, cells);
      if (n.cell == kNoCell) continue;
    } else {
      // Healing is judged from where the unit stands now: its neighbours are the ones it can reach without moving.
      n.target = healTarget(b, u, units);
      if (n.target < 0) continue;
    }
    order = n;
    return true;
  }
  return false;
}

}  // namespace

void considerAbilities(const Battle& b, const Unit& u, Order& order) {
  if (order.kind != ActionType::Wait && order.kind != ActionType::Attack) return;  // a capture, item or supply goes on
  std::vector<int> have;
  b.abilitiesOf(u, have);
  if (have.empty()) return;

  // Heal first (an emergency), unless the planned attack finishes its target.
  if (!plannedKill(b, u, order) && tryFrom(b, u, u.cell, order, false)) return;
  // Summon only instead of a plain Wait or advance (an attack is always worth more), and never by a summoned unit:
  // those could summon in turn without end (every summoner has its own limit).
  if (order.kind != ActionType::Wait || b.isSummoned(u)) return;
  if (order.moveTo != kNoCell && tryFrom(b, u, order.moveTo, order, true)) return;  // after the advance it planned
  tryFrom(b, u, u.cell, order, true);
}

}  // namespace ai
}  // namespace ote
