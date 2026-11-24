// The chance of an ability's condition (ability_conditions_api.h; docs/SRPG.md "Ability conditions"). An ability without "chance" never rolls,
// so a game that does not write the key plays with exactly the random numbers it had.
#include <cmath>

#include "core/battle/battle.h"
#include "core/battle/combat_mods.h"

namespace ote {

bool Battle::abilityInflictRolls(int ability) const {
  if (ability < 0 || ability >= (int)db->abilities.size()) return false;
  for (const AbilityInflict& in : db->abilities[(size_t)ability].inflicts)
    if (in.rolls()) return true;
  return false;
}

int Battle::abilityInflictChance(int user, int ability, int target, Cell from, int entry) const {
  if (ability < 0 || ability >= (int)db->abilities.size()) return -1;
  const AbilityDef& d = db->abilities[(size_t)ability];
  if (entry < 0 || entry >= (int)d.inflicts.size()) return -1;
  const AbilityInflict& in = d.inflicts[(size_t)entry];
  if (!in.rolls()) return 100;
  if (in.chanceExpr < 0) return in.chance;
  if (!formulas || user < 0 || user >= (int)units.size() || target < 0 || target >= (int)units.size()) return 0;
  SkillCtx ctx;
  ctx.unit = user;
  ctx.cell = from;
  ctx.weapon = restingWeapon(units[(size_t)user]);
  ctx.foe = target;
  double v = skillExpr(in.chanceExpr, ctx, 0);
  if (v != v) return 0;  // not a number
  v = std::floor(v);
  return v < 0 ? 0 : v > 100 ? 100 : (int)v;
}

// Every entry in list order, each judged and rolled on its own (docs/SRPG.md "Areas and several effects for abilities"); one entry is exactly the
// old single "inflict".
void Battle::abilityInflict(const AbilityDef& d, int user, int target, std::vector<Event>& events) {
  const int ability = (int)(&d - db->abilities.data());
  for (size_t k = 0; k < d.inflicts.size(); ++k) {
    if (target < 0 || !units[(size_t)target].alive) return;
    const AbilityInflict& in = d.inflicts[k];
    // The same test the staves and items use: a target that already has it for as long is left alone (immunity is
    // reported by inflictCondition itself, as ConditionResisted).
    const int why = inflictWhy(units[(size_t)target], in.cond, in.turns);
    if (why == 2) continue;
    if (why == 0 && in.rolls()) {  // "chance": one roll per target, only where it could take hold
      const int p = abilityInflictChance(user, ability, target, units[(size_t)user].cell, (int)k);
      if (!rollActivation(rng, p)) {
        Event e;
        e.type = EventType::ConditionResisted;
        e.a = target;
        e.b = in.cond;
        e.c = user;
        e.d = p;
        e.flags = kMissed;
        events.push_back(e);
        continue;
      }
    }
    inflictCondition(target, in.cond, in.turns, user, events);
  }
}

}  // namespace ote
