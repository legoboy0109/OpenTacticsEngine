// The computer and the conditions its abilities put on (docs/AI.md "Status abilities"; docs/SRPG.md "Ability conditions").
//
// A condition is worth a share (kConditionShare, as for staves) of the receiver's value (ai_score.cpp unitValue; a leader that decides the game
// counts double) times the chance it takes hold. A HARMFUL condition ("kind" missing or "harmful") is worth something only on a foe, a HELPFUL one
// only on a unit of the user's team; a unit that is immune or has it for as long already is worth nothing, so nothing still on is re-applied.
//
// Cure abilities are worth the same share for a unit they would free of a harmful condition of the user's team, or strip of a helpful one of a
// foe (once per unit; nothing for anything else, so a blessing is never cured from an ally).
//
// Status abilities (an ability with only "inflict") and cure abilities are weighed at every square the unit may stand on, like a staff: the best use beats the order
// already chosen only by its own worth, minus the danger of the square, plus the same base an attack gets. A priced one ("spend") is worth a little
// less. A strike ability's own "inflict" adds its worth to the strike's score (scoreStrike). Charged abilities are not used (as before).
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

}  // namespace

double conditionWorth(const Battle& b, const Unit& user, const Unit& t, int condition, int turns, int chance) {
  if (condition < 0 || chance <= 0 || !t.alive) return 0;
  const bool helpful = b.db->conditions[(size_t)condition].helpful;
  const bool foe = b.isEnemy(user, t);
  if (helpful == foe) return 0;  // a blessing goes to its own team, a curse to a foe
  if (b.inflictWhy(t, condition, turns) != 0) return 0;  // immune, or it has it for as long already
  double v = kConditionShare * unitValue(b, t) * (std::min(chance, 100) / 100.0);
  if (t.leader && b.db->rules.defeatLeaderDead) v *= kLeader;
  return v;
}

double cureWorth(const Battle& b, const Unit& user, const Unit& t, const AbilityDef& d) {
  if (!t.alive || (d.effect != AbilityEffect::Cure && !d.healCures)) return 0;
  const bool foe = b.isEnemy(user, t);
  bool any = false;  // something it takes off that is worth taking: a curse from its own team, a blessing from a foe
  for (const ConditionState& cs : t.conditions)
    any = any || ((d.cureAll || std::find(d.cures.begin(), d.cures.end(), cs.condition) != d.cures.end()) &&
                  b.db->conditions[(size_t)cs.condition].helpful == foe);
  if (!any) return 0;
  double v = kConditionShare * unitValue(b, t);  // once per unit freed, as a staff's cure
  if (t.leader && b.db->rules.defeatLeaderDead) v *= kLeader;
  return v;
}

double inflictsWorth(const Battle& b, const Unit& user, const Unit& t, int ability, Cell from) {
  const AbilityDef& d = b.db->abilities[(size_t)ability];
  double worth = 0;
  for (size_t k = 0; k < d.inflicts.size(); ++k)  // every entry of a list, each by its own chance (one entry = the old single inflict)
    worth += conditionWorth(b, user, t, d.inflicts[k].cond, d.inflicts[k].turns, b.abilityInflictChance(user.id, ability, t.id, from, (int)k));
  return worth;
}

double strikeInflictWorth(const Battle& b, const Unit& u, int ability, const AttackOption& o, Cell s) {
  const AbilityDef& d = b.db->abilities[(size_t)ability];
  if (d.inflicts.empty()) return 0;
  double worth = 0;
  auto add = [&](int target, int hit) {
    const Unit& t = b.units[(size_t)target];
    worth += inflictsWorth(b, u, t, ability, s) * (hit / 100.0);
  };
  if (o.area) {
    AreaForecast f;
    if (!b.abilityStrikeArea(u.id, ability, s, o.cell, f)) return 0;
    for (const AreaVictim& v : f.victims)
      if (v.seen && v.hit.damage < b.units[(size_t)v.unit].hp) add(v.unit, v.hit.hit);  // (a unit the strike defeats gets nothing)
  } else if (o.target >= 0) {
    Forecast f;
    if (!b.abilityStrikeForecast(u.id, ability, o.target, s, f) || !f.attack.can) return 0;
    if (f.attack.totalDamage() < b.units[(size_t)o.target].hp) add(o.target, f.attack.hit);
  }
  return worth;
}

void considerStatusAbilities(const Battle& b, const Unit& u, Cell s, int travel, Order& best) {
  if (b.db->abilities.empty() || b.db->conditions.empty()) return;
  std::vector<AbilityOption> menu;
  b.abilityOptions(u.id, s, menu);
  std::vector<int> targets;
  std::vector<Cell> cells;
  for (const AbilityOption& o : menu) {
    if (!o.ok || b.abilityCharges(o.ability)) continue;
    const AbilityDef& d = b.db->abilities[(size_t)o.ability];
    if (d.area.on()) continue;  // areas: ai_ability_area.cpp
    // A cure, a heal that also cures (worth its cure here; the heal itself is the older emergency rule, ai_abilities.cpp), or only "inflict".
    const bool cure = d.effect == AbilityEffect::Cure || d.healCures;
    if (!cure && (d.effect != AbilityEffect::None || d.inflicts.empty())) continue;
    if (d.free && s != u.cell) continue;  // a free ability is used on the spot, before any move
    b.abilityTargets(u.id, o.ability, s, targets, cells);  // (only units it would change: abilityTargetOk refuses "immune", "has it already", "nothing to cure")
    for (int t : targets) {
      double sc = cure ? cureWorth(b, u, b.units[(size_t)t], d) : inflictsWorth(b, u, b.units[(size_t)t], o.ability, s);
      if (!d.spend.empty()) sc *= kPricedFactor;
      if (sc <= 0) continue;
      sc -= exposureCost(b, u, s);
      if (sc <= 0) continue;
      sc += kAttackBase - kMoveCostWeight * travel;
      if (sc <= best.score) continue;  // strict: an order found before keeps a tie
      best = Order();
      best.score = sc;
      best.kind = ActionType::Ability;
      best.moveTo = s == u.cell || d.free ? kNoCell : s;
      best.ability = o.ability;
      best.target = d.target == AbilityTarget::Self ? -1 : t;
    }
  }
}

}  // namespace ai
}  // namespace ote
