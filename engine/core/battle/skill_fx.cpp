// Small shared pieces of the K2 skill code (docs/SRPG.md "Skills"): the context a fighter's blocks are judged with,
// conditions and chances. Nothing here rolls or changes the battle.
#include <cmath>

#include "core/battle/battle.h"
#include "core/script/formulas.h"

namespace ote {

SkillCtx Battle::skillCombatCtx(const CombatSetup& setup, int unit) const {
  const bool atk = unit == setup.attacker;
  SkillCtx ctx;
  ctx.unit = unit;
  ctx.cell = atk ? setup.from : kNoCell;  // kNoCell = where it really stands
  ctx.weapon = atk ? setup.weaponA : setup.weaponD;
  ctx.foe = atk ? setup.defender : setup.attacker;
  ctx.foeCell = atk ? kNoCell : setup.from;
  ctx.foeWeapon = atk ? setup.weaponD : setup.weaponA;
  ctx.combat = true;
  ctx.initiating = atk;
  return ctx;
}

void Battle::skillCombatList(const CombatSetup& setup, int unit, std::vector<int>& out) const {
  const bool atk = unit == setup.attacker;
  const int w = atk ? setup.weaponA : setup.weaponD;
  skillsOf(units[(size_t)unit], w >= 0 ? w : -2, out);
}

bool Battle::skillBlockHolds(const SkillBlock& block, const SkillCtx& ctx) const {
  for (const SkillCond& c : block.conds)
    if (!skillCondHolds(c, ctx)) return false;
  return true;
}

// a = the owner, b = the foe (nil without one), the usual variables (docs/SRPG.md "Formulas"), `turn` = the round number.
double Battle::skillExpr(int expr, const SkillCtx& ctx, double def) const {
  if (!formulas || ctx.unit < 0 || ctx.unit >= (int)units.size()) return def;
  const Unit& a = units[(size_t)ctx.unit];
  const Unit* b = ctx.foe >= 0 && ctx.foe < (int)units.size() ? &units[(size_t)ctx.foe] : nullptr;
  const Cell ca = ctx.cell == kNoCell ? a.cell : ctx.cell;
  const Cell cb = !b ? kNoCell : (ctx.foeCell == kNoCell ? b->cell : ctx.foeCell);
  Props scratchA, scratchB;
  FormulaCtx fc;
  fc.sides = &sides;
  fc.battle = this;
  fc.a = &a;
  fc.b = b;
  if (ctx.weapon >= 0) fc.w = &db->weapons[(size_t)ctx.weapon];
  if (ctx.foeWeapon >= 0) fc.fw = &db->weapons[(size_t)ctx.foeWeapon];
  if (ca != kNoCell) fc.ta = &numbersAt(ca, scratchA);
  if (cb != kNoCell) fc.tb = &numbersAt(cb, scratchB);
  if (ca != kNoCell && cb != kNoCell) fc.dist = grid.distance(ca, cb);
  if (b && ctx.weapon >= 0) {
    double vs = 1;
    if (versusValue(ctx.weapon, *b, vs)) fc.vs = vs;
  }
  return formulas->evalExpr(expr, fc, def);
}

int Battle::skillEffectNumber(const SkillEffect& e, const SkillCtx& ctx, std::vector<int>& stats) const {
  using K = SkillEffect::Kind;
  auto whole = [](double v, int lo, int hi) {
    if (v != v) return 0;
    v = std::floor(v);
    return v < lo ? lo : v > hi ? hi : (int)v;
  };
  if (e.kind == K::Stats) {
    stats = e.stats;
    for (size_t i = 0; i < e.statExprs.size() && i < stats.size(); ++i)
      if (e.statExprs[i] >= 0) stats[i] = whole(skillExpr(e.statExprs[i], ctx, 0), -1000, 1000);
    return 0;
  }
  if (e.expr < 0) return e.value;
  const bool percent = e.kind == K::DamagePercent || e.kind == K::DamageTakenPercent;
  return percent ? whole(skillExpr(e.expr, ctx, 100), 0, 1000) : whole(skillExpr(e.expr, ctx, 0), -1000, 1000);
}

bool Battle::unitHasKey(const Unit& u, const std::string& key) const {
  if (unitTypeMatchesKey(typeOf(u), key)) return true;
  if (u.character >= 0 && (size_t)u.character < db->characters.size()) {
    const std::vector<std::string>& tags = db->characters[(size_t)u.character].tags;
    for (const std::string& t : tags)
      if (t == key) return true;
  }
  return false;
}

int Battle::skillChance(const SkillBlock& block, const SkillCtx& ctx) const {
  double v = block.chance;
  if (block.chanceExpr >= 0) {
    if (!formulas || ctx.unit < 0 || ctx.unit >= (int)units.size()) return 0;
    v = skillExpr(block.chanceExpr, ctx, 0);
  }
  if (v != v) return 0;  // not a number
  v = std::floor(v);
  return v < 0 ? 0 : v > 100 ? 100 : (int)v;
}

}  // namespace ote
