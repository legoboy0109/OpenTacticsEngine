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

int Battle::skillChance(const SkillBlock& block, const SkillCtx& ctx) const {
  double v = block.chance;
  if (block.chanceExpr >= 0) {
    if (!formulas || ctx.unit < 0 || ctx.unit >= (int)units.size()) return 0;
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
    if (ca != kNoCell) fc.ta = &numbersAt(ca, scratchA);
    if (cb != kNoCell) fc.tb = &numbersAt(cb, scratchB);
    if (ca != kNoCell && cb != kNoCell) fc.dist = grid.distance(ca, cb);
    if (b && ctx.weapon >= 0) {
      double vs = 1;
      if (versusValue(ctx.weapon, *b, vs)) fc.vs = vs;
    }
    v = formulas->evalExpr(block.chanceExpr, fc, 0);
  }
  if (v != v) return 0;  // not a number
  v = std::floor(v);
  return v < 0 ? 0 : v > 100 ? 100 : (int)v;
}

}  // namespace ote
