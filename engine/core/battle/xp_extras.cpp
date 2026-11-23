// Experience options (docs/SRPG.md "Experience: options"): a fight that did no damage, experience for abilities, and the
// condition an ability inflicts. All of it is off in a game that does not write the keys, so nothing here runs for it.
#include <algorithm>
#include <cmath>

#include "core/battle/battle.h"
#include "core/script/formulas.h"

namespace ote {

int Battle::levelCap(const Unit& u) const {
  const int own = typeOf(u).maxLevel;
  return own >= 0 ? own : db->rules.maxLevel;
}

int Battle::xpNoDamage(int unit, int foe) const {
  const int expr = db->rules.xpNoDamageExpr;
  if (expr < 0 || !formulas) return 0;
  FormulaCtx ctx;
  ctx.sides = &sides;
  ctx.battle = this;
  ctx.a = &units[(size_t)unit];
  ctx.b = &units[(size_t)foe];
  return (int)std::floor(formulas->evalExpr(expr, ctx, 0));
}

void Battle::abilityExperience(const AbilityDef& d, int user, int target, std::vector<Event>& events) {
  if (!db->rules.progression || !units[(size_t)user].alive) return;
  int xp = d.xp;
  if (d.xpExpr >= 0 && formulas) {
    FormulaCtx ctx;
    ctx.sides = &sides;
    ctx.battle = this;
    ctx.a = &units[(size_t)user];
    ctx.b = target >= 0 ? &units[(size_t)target] : nullptr;
    xp = (int)std::floor(formulas->evalExpr(d.xpExpr, ctx, 0));
  }
  if (xp > 0) awardXp(user, std::min(xp, kMaxStaffXp), events, false);
}

}  // namespace ote
