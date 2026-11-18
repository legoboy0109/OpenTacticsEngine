// Menu abilities (docs/SRPG.md "AI"): the AI summons, heals, cures a condition (itself or an ally next to it), refreshes an ally that has already
// acted and still has a foe to hit, and a unit marked "goesFor": ["steal"] steals. Push, pull, swap, moveBehind and movePast are never used.
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
         f.attack.totalDamage() >= b.units[(size_t)o.target].hp;
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
    if (summon && d.summon.replace && b.summonCount(u.id, o.ability) >= d.summon.limit) continue;  // the computer never throws away a summon to make another
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

// A unit marked "steal" (docs/SRPG.md "Steal"): from `from`, the first steal ability that works on a visible neighbouring foe, taking the
// first item it may take. Foes in unit-id order, items in slot order, so the same battle always gives the same choice.
bool tryStealFrom(const Battle& b, const Unit& u, Cell from, Order& order) {
  std::vector<AbilityOption> menu;
  b.abilityOptions(u.id, from, menu);
  for (const AbilityOption& o : menu) {
    if (!o.ok) continue;
    const AbilityDef& d = b.db->abilities[(size_t)o.ability];
    if (d.effect != AbilityEffect::Steal || (d.free && from != u.cell)) continue;
    std::vector<int> targets;
    std::vector<Cell> cells;
    b.abilityTargets(u.id, o.ability, from, targets, cells);
    for (int id : targets) {
      std::vector<StealOption> items;
      b.stealOptions(u.id, o.ability, from, id, items);
      for (const StealOption& it : items) {
        if (!it.ok) continue;
        Order n;
        n.score = order.score;
        n.kind = ActionType::Ability;
        n.ability = o.ability;
        n.target = id;
        n.slot = it.slot;
        n.moveTo = (from == u.cell || d.free) ? kNoCell : from;
        order = n;
        return true;
      }
    }
  }
  return false;
}

// Cure (docs/SRPG.md "Cure as an ability"): from `from`, the first cure ability that has a unit to cure: the unit itself, else an ally
// (unit-id order). What an ability cures is the maker's own list, so like a staff's cure the AI does not ask whether a condition is a curse.
bool tryCureFrom(const Battle& b, const Unit& u, Cell from, Order& order) {
  std::vector<AbilityOption> menu;
  b.abilityOptions(u.id, from, menu);
  for (const AbilityOption& o : menu) {
    if (!o.ok) continue;
    const AbilityDef& d = b.db->abilities[(size_t)o.ability];
    if (d.effect != AbilityEffect::Cure || (d.free && from != u.cell)) continue;
    std::vector<int> targets;
    std::vector<Cell> cells;
    b.abilityTargets(u.id, o.ability, from, targets, cells);
    int pick = -1;
    for (int id : targets)
      if (b.units[(size_t)id].alive && !b.isEnemy(u, b.units[(size_t)id]) && (pick < 0 || id == u.id)) pick = id;
    if (pick < 0) continue;
    Order n;
    n.score = order.score;
    n.kind = ActionType::Ability;
    n.ability = o.ability;
    n.target = pick;
    n.moveTo = (from == u.cell || d.free) ? kNoCell : from;
    order = n;
    return true;
  }
  return false;
}

// Refresh (docs/SRPG.md "Refresh an ally"): an ally that has used its action and has a foe it could hit from where it stands. The ability's own
// per-turn limits (a unit is refreshed once, a refresher cannot be refreshed back) keep two of them from looping.
bool tryRefreshFrom(const Battle& b, const Unit& u, Cell from, Order& order) {
  std::vector<AbilityOption> menu;
  b.abilityOptions(u.id, from, menu);
  for (const AbilityOption& o : menu) {
    if (!o.ok) continue;
    const AbilityDef& d = b.db->abilities[(size_t)o.ability];
    if (d.effect != AbilityEffect::Refresh || (d.free && from != u.cell)) continue;
    std::vector<int> targets;
    std::vector<Cell> cells;
    b.abilityTargets(u.id, o.ability, from, targets, cells);
    for (int id : targets) {
      const Unit& t = b.units[(size_t)id];
      if (id == u.id || !t.alive || t.cell == kNoCell) continue;
      std::vector<AttackOption> hits;
      b.attackOptions(id, t.cell, hits);
      if (hits.empty()) continue;
      Order n;
      n.score = order.score;
      n.kind = ActionType::Ability;
      n.ability = o.ability;
      n.target = id;
      n.moveTo = (from == u.cell || d.free) ? kNoCell : from;
      order = n;
      return true;
    }
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
  // Cure a unit, or refresh an ally that can still fight, instead of a plain Wait or a hit that does not finish its target.
  if (!plannedKill(b, u, order)) {
    const Cell at = order.moveTo == kNoCell ? u.cell : order.moveTo;
    if (tryCureFrom(b, u, at, order) || tryRefreshFrom(b, u, at, order)) return;
  }
  // A unit marked "steal" takes something from a neighbouring foe instead of a plain Wait or a hit that does not finish its target.
  if ((u.aiGoals & kAiSteal) && !plannedKill(b, u, order)) {
    const Cell at = order.moveTo == kNoCell ? u.cell : order.moveTo;
    if (tryStealFrom(b, u, at, order)) return;
  }
  // Summon only instead of a plain Wait or advance (an attack is always worth more), and never by a summoned unit:
  // those could summon in turn without end (every summoner has its own limit).
  if (order.kind != ActionType::Wait || b.isSummoned(u)) return;
  if (order.moveTo != kNoCell && tryFrom(b, u, order.moveTo, order, true)) return;  // after the advance it planned
  tryFrom(b, u, u.cell, order, true);
}

}  // namespace ai
}  // namespace ote
