// The AI and abilities that strike (docs/AI.md "Abilities that strike"): a strike ability is weighed exactly as an attack with its weapon would be
// (ai_score.cpp, inside a Battle::StrikeScope so the forecasts are the ability's: no counter-attack), and wins over the unit's own attacks only by
// a better score. A priced one ("spend") is worth a little less, so a free attack that does as well is kept and the pool lasts.
#include <algorithm>

#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

namespace {

constexpr double kAttackBase = 200;      // as an attack's (ai_order.cpp)
constexpr double kMoveCostWeight = 0.5;  // as an attack's
constexpr double kPricedFactor = 0.9;    // a strike that costs MP (or a side resource) against one that costs nothing

}  // namespace

void strikeAbilitiesAt(const Battle& b, const Unit& u, Cell s, bool charged, std::vector<int>& out) {
  out.clear();
  if (b.db->abilities.empty()) return;
  std::vector<AbilityOption> opts;
  b.abilityOptions(u.id, s, opts);  // usable (price, uses, wait, "afterMove") and something in reach
  for (const AbilityOption& o : opts)
    if (o.ok && b.abilityStrikeWeapon(o.ability) >= 0 && b.abilityCharges(o.ability) == charged) out.push_back(o.ability);
}

void strikeOptionsFor(const Battle& b, const Unit& u, int ability, Cell s, std::vector<AttackOption>& out) {
  b.abilityStrikeOptions(u.id, ability, s, out);
  std::vector<int> units;
  std::vector<Cell> cells;
  b.abilityTargets(u.id, ability, s, units, cells);
  out.erase(std::remove_if(out.begin(), out.end(),
                           [&](const AttackOption& o) {
                             return o.area ? std::find(cells.begin(), cells.end(), o.cell) == cells.end()
                                           : std::find(units.begin(), units.end(), o.target) == units.end();
                           }),
            out.end());
}

double scoreStrike(const Battle& b, const Unit& u, int ability, const AttackOption& o, Cell s) {
  Battle::StrikeScope scope(b, b.abilityStrikeWeapon(ability));
  double sc = o.area ? scoreAreaAttack(b, u, o.weapon, o.cell, s) : scoreAttack(b, u, b.units[(size_t)o.target], o.weapon, s);
  if (!b.db->abilities[(size_t)ability].spend.empty()) sc *= kPricedFactor;
  return sc;
}

void considerStrikes(const Battle& b, const Unit& u, Cell s, int travel, const Stance& st, Order& best) {
  std::vector<int> strikes;
  strikeAbilitiesAt(b, u, s, false, strikes);
  std::vector<AttackOption> options;
  for (int a : strikes) {
    strikeOptionsFor(b, u, a, s, options);
    for (const AttackOption& o : options) {
      double sc = scoreStrike(b, u, a, o, s);
      if (sc <= 0) continue;
      sc -= exposureCost(b, u, s);
      const bool judged = o.target >= 0 && b.isEnemy(u, b.units[(size_t)o.target]);
      if (sc <= 0 || (judged && !adjustAttack(b, st, u, b.units[(size_t)o.target], s, sc))) continue;
      sc += kAttackBase - kMoveCostWeight * travel;
      if (sc <= best.score) continue;  // strict: an attack of its own keeps a tie
      best = Order();
      best.score = sc;
      best.kind = ActionType::Ability;
      best.moveTo = s == u.cell ? kNoCell : s;
      best.ability = a;
      best.target = o.area ? -1 : o.target;
      best.cell = o.area ? o.cell : kNoCell;
    }
  }
}

}  // namespace ai
}  // namespace ote
