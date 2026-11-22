// Abilities that strike and heal by formula (strike_api.h / strike_internal.h; docs/SRPG.md "Abilities that strike", "Healing by formula"). The attack
// itself is the normal one (combat.cpp, area_attack.cpp) run inside a StrikeScope.
#include <algorithm>
#include <cmath>

#include "core/battle/battle.h"
#include "core/script/formulas.h"

namespace ote {

namespace {

bool refuse(std::string* why, const std::string& reason) {
  if (why) *why = reason;
  return false;
}

}  // namespace

int Battle::abilityStrikeWeapon(int ability) const {
  if (ability < 0 || ability >= (int)db->abilities.size()) return -1;
  const AbilityDef& d = db->abilities[(size_t)ability];
  return d.effect == AbilityEffect::Strike ? d.strike : -1;
}

void Battle::abilityStrikeOptions(int unit, int ability, Cell from, std::vector<AttackOption>& out) const {
  out.clear();
  const int w = abilityStrikeWeapon(ability);
  if (w < 0) return;
  StrikeScope scope(*this, w);
  attackOptions(unit, from, out);
}

bool Battle::abilityStrikeForecast(int unit, int ability, int target, Cell from, Forecast& out) const {
  const int w = abilityStrikeWeapon(ability);
  if (w < 0 || isAreaWeapon(w)) return false;
  StrikeScope scope(*this, w);
  CombatSetup setup;
  return forecastFight(unit, target, w, from, out, setup);
}

bool Battle::abilityStrikeArea(int unit, int ability, Cell from, Cell target, AreaForecast& out) const {
  const int w = abilityStrikeWeapon(ability);
  if (w < 0 || !isAreaWeapon(w)) return false;
  StrikeScope scope(*this, w);
  return areaForecast(unit, w, from, target, out);
}

bool Battle::strikeTargetOk(const Unit& u, int ability, Cell from, int target, Cell cell, std::string* why) const {
  const int w = abilityStrikeWeapon(ability);
  if (isAreaWeapon(w)) {
    if (!grid.valid(cell)) return refuse(why, "that square is not on the map");
    AreaForecast f;
    if (!abilityStrikeArea(u.id, ability, from, cell, f)) return refuse(why, "there is nothing to strike there");
    return true;
  }
  if (target < 0 || target >= (int)units.size() || !units[(size_t)target].alive) return refuse(why, "that is not a unit on the board");
  const Unit& t = units[(size_t)target];
  if (!unitVisibleTo(teamOf(u), t)) return refuse(why, "you cannot see that unit");
  if (!isEnemy(u, t)) return refuse(why, "that ability works on an enemy");
  Forecast f;
  if (!abilityStrikeForecast(u.id, ability, target, from, f)) return refuse(why, "that unit is out of reach");
  return true;
}

void Battle::applyStrikeAbility(const Action& action, std::vector<Event>& events, bool resolving) {
  const int uid = action.unit;
  const int ability = action.ability;
  const AbilityDef& def = db->abilities[(size_t)ability];
  const int w = def.strike;
  const bool area = isAreaWeapon(w);
  Event used;
  used.type = EventType::AbilityUsed;
  used.a = uid;
  used.b = ability;
  used.c = area ? -1 : action.target;
  used.d = area ? action.cell : -1;
  events.push_back(used);

  Action atk = Action::attack(uid, area ? occupant[(size_t)action.cell] : action.target, w);
  if (area) atk.cell = action.cell;
  const size_t from = events.size();
  {
    StrikeScope scope(*this, w);
    applyAttack(atk, events);
  }
  // The ability's own "inflict": on every unit one of its strikes hit (not a counter: there is none), if it still stands. In the order struck.
  std::vector<int> hit;
  for (size_t i = from; i < events.size(); ++i) {
    const Event& e = events[i];
    if (e.type == EventType::Attack && e.a == uid && !(e.flags & kCounter) && (e.flags & kHit) && std::find(hit.begin(), hit.end(), e.b) == hit.end())
      hit.push_back(e.b);
  }
  if (def.inflictCond >= 0)
    for (int t : hit)
      if (units[(size_t)t].alive && !over) abilityInflict(def, uid, t, events);
  if (units[(size_t)uid].alive) abilityExperience(def, uid, area ? (hit.empty() ? -1 : hit[0]) : action.target, events);
  if (!resolving) {  // a charged ability paid for these when it was declared
    noteAbilityUse(uid, ability);
    spendAbilityItem(uid, ability, events);
    payAbilityPrice(uid, ability, events);
    if (units[(size_t)uid].alive && !over) endOrder(uid, true);  // it was a fight ("moveAgain": "notAfterAttack" counts it)
  }
}

int Battle::abilityHealAmount(const AbilityDef& d, int user, int target) const {
  const Unit& t = units[(size_t)target];
  if (d.healPercent > 0) return std::max(1, maxHp(t) * d.healPercent / 100);
  if (d.healExpr >= 0) {
    FormulaCtx ctx;
    ctx.sides = &sides;
    ctx.battle = this;
    ctx.a = &units[(size_t)user];
    ctx.b = &t;
    const double v = formulas ? formulas->evalExpr(d.healExpr, ctx, 0) : 0;
    return (int)std::max(0.0, std::min(1000000.0, std::floor(v)));
  }
  return d.amount;
}

}  // namespace ote
