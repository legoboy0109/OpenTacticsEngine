// Repair (docs/ECONOMY.md "Repair and supply"): at the start of a side's turn every hurt unit standing on a structure
// of its own side heals rules.supply.repair.amount percent of its maximum HP and pays for it.
//
// Money: the price of healing h HP is, for each resource of the unit type's cost,
//   ceil(cost x costShare / 100 x h / maxHp)
// (rounded up per resource, so a repair is never free by rounding; costShare 0 or a type without cost = free).
// If the side cannot pay the whole amount the unit heals the most HP it can afford, possibly none (then nothing
// happens and no event is emitted). Units are served in unit index order, so what is left goes to the first ones.
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

namespace {

// ceil(cost x share% x heal / maxHp). The product can overflow 64 bits only for absurd hit point totals
// (cost < 2e9, share <= 100, so cost x share < 2.1e11 needs heal, maxHp <= ~4e7); those are scaled down
// together first, which changes the price by far less than one unit of money.
long long repairPrice(long long cost, int share, int heal, int maxHp) {
  long long h = heal, m = maxHp;
  if (m > 10000000) {
    long long s = (m + 9999999) / 10000000;
    m = (m + s - 1) / s;
    h = std::min(m, (h + s - 1) / s);
  }
  return (cost * share * h + 100 * m - 1) / (100 * m);
}

}  // namespace

bool Battle::repairPlan(const Unit& u, Cell at, RepairPlan& out) const {
  out = RepairPlan();
  const SupplyRules& rules = db->rules.supply;
  if (!rules.repair || !u.alive || !grid.valid(at)) return false;
  int si = structureAt[(size_t)at];
  if (si < 0 || structures[(size_t)si].owner != u.side) return false;
  const StructureDef& def = defOf(structures[(size_t)si]);
  if (!def.repairsAll) {
    const UnitTypeDef& t = typeOf(u);
    bool takes = false;
    for (const std::string& key : def.repairs) {
      takes = takes || key == t.id;
      for (const std::string& tag : t.tags) takes = takes || key == tag;
    }
    if (!takes) return false;
  }
  const int maxHp = this->maxHp(u);
  if (maxHp <= 0 || u.hp >= maxHp) return false;
  // Percent of the maximum, rounded up, never more than is missing.
  const int want = std::min(maxHp - u.hp, (int)(((long long)maxHp * rules.repairAmount + 99) / 100));

  const std::vector<int>& funds = sides[(size_t)u.side].resources;
  const std::vector<Amount>& price = typeOf(u).cost;
  auto costOf = [&](int heal, std::vector<int>& cost) {
    cost.assign(db->resources.size(), 0);
    if (rules.repairCostShare <= 0) return;
    for (const Amount& a : price)
      if (a.amount > 0) cost[(size_t)a.resource] += (int)repairPrice(a.amount, rules.repairCostShare, heal, maxHp);
  };
  auto affordable = [&](int heal) {
    std::vector<int> cost;
    costOf(heal, cost);
    for (size_t r = 0; r < cost.size(); ++r)
      if (cost[r] > funds[r]) return false;
    return true;
  };
  // The cost only grows with the amount, so the largest affordable amount can be searched for.
  int lo = 0, hi = want;
  while (lo < hi) {
    int mid = lo + (hi - lo + 1) / 2;
    if (affordable(mid)) lo = mid;
    else hi = mid - 1;
  }
  if (lo <= 0) return false;
  out.heal = lo;
  costOf(lo, out.cost);
  return true;
}

bool Battle::repairPreviewAt(int unit, Cell at, RepairPlan& out) const {
  out = RepairPlan();
  if (unit < 0 || unit >= (int)units.size()) return false;
  return repairPlan(units[(size_t)unit], at, out);
}

bool Battle::repairPreview(int unit, RepairPlan& out) const {
  out = RepairPlan();
  if (unit < 0 || unit >= (int)units.size()) return false;
  return repairPlan(units[(size_t)unit], units[(size_t)unit].cell, out);
}

void Battle::repairTurn(std::vector<Event>& events) {
  if (!db->rules.supply.repair) return;
  for (size_t i = 0; i < units.size(); ++i) {
    Unit& u = units[i];
    if (!u.alive || !ownsTurn(u)) continue;
    RepairPlan plan;
    if (!repairPlan(u, u.cell, plan)) continue;
    bool paid = false;
    for (size_t r = 0; r < plan.cost.size(); ++r) {
      changeResource(activeSide, (int)r, -plan.cost[r], events);
      paid = paid || plan.cost[r] > 0;
    }
    u.hp += plan.heal;
    Event e;
    e.type = EventType::UnitRepaired;
    e.a = (int)i;
    e.b = plan.heal;
    e.c = u.hp;
    if (paid) e.data = plan.cost;
    events.push_back(e);
  }
}

}  // namespace ote
