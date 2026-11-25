// The computer and abilities with an area (docs/AI.md "Abilities with an area"; docs/SRPG.md "Areas and several effects for abilities").
//
// Each square the ability may be aimed at from a square the unit may stand on is scored by the SUM over the units it would act on that the unit's
// team sees (Battle::abilityAreaForecast; units in fog are acted on by luck, never counted):
//  - HP restored: the share of the unit's maximum times its value (ai_score.cpp unitValue), as a staff's heal (ai_staves.cpp);
//  - a revive: reviveWorth (ai_revive.cpp; docs/SRPG.md "Downed units and revive");
//  - a cure: a share (kConditionShare) of the unit's value once when it frees its own team of a harmful condition or strips a foe of a helpful one;
//  - each "inflict" entry: conditionWorth (the condition's kind decides the side, times its chance).
// Whatever helps a foe or harms its own team counts AGAINST the use with the same weight (a "unit" heal that also heals two foes, a "unit" curse that
// catches an ally), so a total of 0 or less is never used: the area is never wasted. A leader whose defeat ends the game counts double.
// The best total beats the order already chosen only by its own worth, minus the danger of the square, plus the base an attack gets, as for the
// status abilities (ai_conditions.cpp). A priced one ("spend") is worth 10 % less. Charged abilities are not used (as before for every ability).
#include <algorithm>

#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

namespace {

constexpr double kAttackBase = 200;       // as an attack's (ai_order.cpp)
constexpr double kMoveCostWeight = 0.5;   // as an attack's
constexpr double kConditionShare = 0.25;  // as a staff's (ai_staves.cpp)
constexpr double kLeader = 2.0;           // as a staff's
constexpr double kPricedFactor = 0.9;     // as a strike's (ai_strike.cpp)

double leaderFactor(const Battle& b, const Unit& t) { return t.leader && b.db->rules.defeatLeaderDead ? kLeader : 1.0; }

// What a heal / cure part does for `t` (signed: + for the user's own team, - for a foe).
double healCureWorth(const Battle& b, const Unit& user, const Unit& t, const AbilityDef& d, const AbilityAreaHit& h) {
  const bool foe = b.isEnemy(user, t);
  double v = 0;
  if (h.revive > 0) v += reviveWorth(b, user, t, h.revive);  // signed already (ai_revive.cpp)
  if (h.heal > 0) {
    const double w = h.heal / (double)std::max(1, b.maxHp(t)) * unitValue(b, t) * leaderFactor(b, t);
    v += foe ? -w : w;
  }
  if (h.cures > 0) {
    bool good = false, bad = false;  // takes off a curse of its own team / a blessing of a foe (good); the opposite (bad)
    for (const ConditionState& cs : t.conditions) {
      if (!d.cureAll && std::find(d.cures.begin(), d.cures.end(), cs.condition) == d.cures.end()) continue;
      (b.db->conditions[(size_t)cs.condition].helpful == foe ? good : bad) = true;
    }
    const double w = kConditionShare * unitValue(b, t) * leaderFactor(b, t);
    v += (good ? w : 0) - (bad ? w : 0);
  }
  return v;
}

// Each "inflict" entry on `t`, signed: conditionWorth on the right side, its negative on the wrong one (a curse on its own team, a blessing on a foe).
double signedInflictWorth(const Battle& b, const Unit& user, const Unit& t, int ability, Cell s) {
  const AbilityDef& d = b.db->abilities[(size_t)ability];
  double v = 0;
  for (size_t k = 0; k < d.inflicts.size(); ++k) {
    const AbilityInflict& in = d.inflicts[k];
    const int chance = b.abilityInflictChance(user.id, ability, t.id, s, (int)k);
    const double right = conditionWorth(b, user, t, in.cond, in.turns, chance);
    if (right > 0) {
      v += right;
      continue;
    }
    // conditionWorth says 0 on the wrong side; the cost there is the same share, when it would take hold at all.
    if (chance <= 0 || b.inflictWhy(t, in.cond, in.turns) != 0) continue;
    if (b.db->conditions[(size_t)in.cond].helpful == b.isEnemy(user, t))
      v -= kConditionShare * unitValue(b, t) * (std::min(chance, 100) / 100.0) * leaderFactor(b, t);
  }
  return v;
}

}  // namespace

void considerAreaAbilities(const Battle& b, const Unit& u, Cell s, int travel, Order& best) {
  // Cheap ways out first: this runs for every square of every unit, and most games have no area ability at all.
  bool any = false;
  for (const AbilityDef& d : b.db->abilities) any = any || d.area.on();
  if (!any) return;
  std::vector<int> have;
  b.abilitiesOf(u, have);
  any = false;
  for (int a : have) any = any || b.db->abilities[(size_t)a].area.on();
  if (!any) return;
  std::vector<AbilityOption> menu;
  b.abilityOptions(u.id, s, menu);
  std::vector<int> targets;
  std::vector<Cell> cells;
  for (const AbilityOption& o : menu) {
    if (!o.ok || b.abilityCharges(o.ability)) continue;
    const AbilityDef& d = b.db->abilities[(size_t)o.ability];
    if (!d.area.on()) continue;
    if (d.free && s != u.cell) continue;  // a free ability is used on the spot, before any move
    b.abilityTargets(u.id, o.ability, s, targets, cells);  // the squares with at least one seen unit it acts on (ascending)
    for (Cell c : cells) {
      AbilityAreaForecast f;
      if (!b.abilityAreaForecast(u.id, o.ability, s, c, f)) continue;
      double sc = 0;
      for (const AbilityAreaHit& h : f.hits) {
        if (!h.seen) continue;
        const Unit& t = b.units[(size_t)h.unit];
        sc += healCureWorth(b, u, t, d, h) + signedInflictWorth(b, u, t, o.ability, s);
      }
      if (!d.spend.empty()) sc *= kPricedFactor;
      if (sc <= 0) continue;  // never wasted: it must do more good than harm
      sc -= exposureCost(b, u, s);
      if (sc <= 0) continue;
      sc += kAttackBase - kMoveCostWeight * travel;
      if (sc <= best.score) continue;  // strict: an order found before keeps a tie (so the lowest square of equals wins)
      best = Order();
      best.score = sc;
      best.kind = ActionType::Ability;
      best.moveTo = s == u.cell || d.free ? kNoCell : s;
      best.ability = o.ability;
      best.cell = c;
    }
  }
}

}  // namespace ai
}  // namespace ote
