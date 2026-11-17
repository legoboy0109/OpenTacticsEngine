// The computer side of repair and supply (docs/ECONOMY.md "AI"). Simple and deterministic, and it only ever
// picks squares from the unit's own reach, which already respects its fuel (Battle::reachable), so it never
// plans a move the tank does not allow. Nothing here runs in a game without the rules.
//  * A unit below 40% HP that can reach a structure of its own side which would repair it (and could pay for at
//    least some of it) goes there, unless its planned attack kills.
//  * A unit below a quarter of its fuel, or with no usable weapon left for lack of ammunition, goes toward the
//    nearest own structure (when structures resupply) or own supplier; once there it waits to be filled up.
//  * A supplier unit uses Supply when a square it can reach has a unit of its side next to it that needs it.
#include <algorithm>

#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

namespace {

constexpr int kRetreatPercent = 40;
constexpr double kRetreatScore = 100;   // after attacks (>= 200), level with a good advance
constexpr double kSupplyScore = 150;    // a supplier with somebody to fill beats advancing, not attacking

bool planKills(const Battle& b, const Unit& u, const Order& order) {
  if (order.kind != ActionType::Attack) return false;
  Cell from = order.moveTo == kNoCell ? u.cell : order.moveTo;
  Forecast f;
  if (!b.forecast(u.id, order.target, order.weapon, from, f)) return false;
  return f.attack.totalDamage() >= b.units[(size_t)order.target].hp;
}

Order stayOrRetreat(const Unit& u, Cell to, double score) {
  Order o;
  o.score = score;
  o.moveTo = to == u.cell ? kNoCell : to;
  o.kind = ActionType::Wait;
  return o;
}

bool considerRepair(const Battle& b, const Unit& u, const Reach& reach, const std::vector<Cell>& stops, Order& order) {
  if (!b.db->rules.supply.repair) return false;
  if (u.hp * 100 >= kRetreatPercent * b.maxHp(u)) return false;
  if (planKills(b, u, order)) return false;
  Cell best = kNoCell;
  int bestCost = kFar;
  RepairPlan plan;
  for (Cell s : stops) {  // ascending cell index: the first of equal trips wins
    int cost = s == u.cell ? 0 : reach.cost[(size_t)s];
    if (cost < bestCost && b.repairPreviewAt(u.id, s, plan)) {
      best = s;
      bestCost = cost;
    }
  }
  if (best == kNoCell) return false;
  order = stayOrRetreat(u, best, kRetreatScore);
  return true;
}

bool lowOnSupplies(const Battle& b, const Unit& u) {
  if (u.fuel >= 0 && u.fuel * 4 < b.maxFuel(u.id)) return true;
  if (u.ammo.empty()) return false;
  std::vector<int> weapons;
  b.weaponsOf(u, weapons);
  return weapons.empty();  // every weapon is out of ammunition
}

bool considerResupply(const Battle& b, const Unit& u, const std::vector<Cell>& stops, Order& order) {
  if (!lowOnSupplies(b, u) || planKills(b, u, order)) return false;
  std::vector<Cell> goals;
  bool covered = false;  // it already stands where the next turn start fills it up
  if (b.db->rules.supply.structures) {
    std::vector<Cell> footprint;
    for (size_t s = 0; s < b.structures.size(); ++s) {
      if (knownOwner(b, u.side, (int)s) != u.side) continue;
      b.structureCells((int)s, footprint);
      for (Cell c : footprint) {
        goals.push_back(c);
        covered = covered || c == u.cell;
      }
    }
  }
  for (const Unit& o : b.units)
    if (o.alive && o.id != u.id && o.side == u.side && b.typeOf(o).supplies) {
      goals.push_back(o.cell);
      covered = covered || b.grid.distance(o.cell, u.cell) == 1;
    }
  if (goals.empty()) return false;
  if (covered) {
    order = stayOrRetreat(u, u.cell, kRetreatScore);
    return true;
  }
  std::vector<int> field;
  distanceField(b, b.typeOf(u).moveType, goals, field);
  Cell best = kNoCell;
  int bestVal = field[(size_t)u.cell];
  for (Cell s : stops)
    if (field[(size_t)s] < bestVal) {  // strictly nearer than where it stands (and than any earlier stop)
      best = s;
      bestVal = field[(size_t)s];
    }
  if (best == kNoCell) return false;  // cannot get nearer this turn (or no way there at all)
  order = stayOrRetreat(u, best, kRetreatScore);
  return true;
}

void considerSupplying(const Battle& b, const Unit& u, const Reach& reach, const std::vector<Cell>& stops, Order& order) {
  if (!b.typeOf(u).supplies) return;
  std::vector<int> targets;
  for (Cell s : stops) {
    b.supplyTargetsFrom(u.id, s, targets);
    if (targets.empty()) continue;
    double score = kSupplyScore - (s == u.cell ? 0 : reach.cost[(size_t)s]);
    if (score <= order.score) continue;
    order = Order();
    order.score = score;
    order.moveTo = s == u.cell ? kNoCell : s;
    order.kind = ActionType::Supply;
  }
}

}  // namespace

void considerSupply(const Battle& b, const Unit& u, const Reach& reach, const std::vector<Cell>& stops, Order& order) {
  if (considerRepair(b, u, reach, stops, order)) return;
  if (considerResupply(b, u, stops, order)) return;
  considerSupplying(b, u, reach, stops, order);
}

}  // namespace ai
}  // namespace ote
