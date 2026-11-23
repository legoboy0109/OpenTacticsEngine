// The chance of an ability's condition (ability_conditions_api.h; docs/SRPG.md "Ability conditions"). An ability without "chance" never rolls,
// so a game that does not write the key plays with exactly the random numbers it had.
#include <cmath>

#include "core/battle/battle.h"
#include "core/battle/combat_mods.h"

namespace ote {

bool Battle::abilityInflictRolls(int ability) const {
  if (ability < 0 || ability >= (int)db->abilities.size()) return false;
  const AbilityDef& d = db->abilities[(size_t)ability];
  return d.inflictCond >= 0 && (d.inflictChance > 0 || d.inflictChanceExpr >= 0);
}

int Battle::abilityInflictChance(int user, int ability, int target, Cell from) const {
  if (ability < 0 || ability >= (int)db->abilities.size()) return -1;
  const AbilityDef& d = db->abilities[(size_t)ability];
  if (d.inflictCond < 0) return -1;
  if (!abilityInflictRolls(ability)) return 100;
  if (d.inflictChanceExpr < 0) return d.inflictChance;
  if (!formulas || user < 0 || user >= (int)units.size() || target < 0 || target >= (int)units.size()) return 0;
  SkillCtx ctx;
  ctx.unit = user;
  ctx.cell = from;
  ctx.weapon = restingWeapon(units[(size_t)user]);
  ctx.foe = target;
  double v = skillExpr(d.inflictChanceExpr, ctx, 0);
  if (v != v) return 0;  // not a number
  v = std::floor(v);
  return v < 0 ? 0 : v > 100 ? 100 : (int)v;
}

void Battle::abilityInflict(const AbilityDef& d, int user, int target, std::vector<Event>& events) {
  if (d.inflictCond < 0 || target < 0 || !units[(size_t)target].alive) return;
  // The same test the staves and items use: a target that already has it for as long is left alone (immunity is
  // reported by inflictCondition itself, as ConditionResisted).
  const int why = inflictWhy(units[(size_t)target], d.inflictCond, d.inflictTurns);
  if (why == 2) return;
  const int ability = (int)(&d - db->abilities.data());
  if (why == 0 && abilityInflictRolls(ability)) {  // "chance": one roll per target, only where it could take hold
    const int p = abilityInflictChance(user, ability, target, units[(size_t)user].cell);
    if (!rollActivation(rng, p)) {
      Event e;
      e.type = EventType::ConditionResisted;
      e.a = target;
      e.b = d.inflictCond;
      e.c = user;
      e.d = p;
      e.flags = kMissed;
      events.push_back(e);
      return;
    }
  }
  inflictCondition(target, d.inflictCond, d.inflictTurns, user, events);
}

}  // namespace ote
