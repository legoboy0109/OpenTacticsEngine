// The one condition evaluator of skill blocks (and, later, menu abilities): does a condition hold for this owner and
// foe? docs/SRPG.md "Skills": CONDITIONS. No randomness and no side effects, so forecasts and menus can ask freely.
#include <algorithm>

#include "core/battle/battle.h"
#include "core/script/formulas.h"

namespace ote {

namespace {

bool cmp(CmpOp op, long long a, long long b) {
  switch (op) {
    case CmpOp::Eq: return a == b;
    case CmpOp::Ne: return a != b;
    case CmpOp::Lt: return a < b;
    case CmpOp::Le: return a <= b;
    case CmpOp::Gt: return a > b;
    case CmpOp::Ge: return a >= b;
  }
  return false;
}

bool hasKey(const std::vector<std::string>& tags, const std::string& key) {
  return std::find(tags.begin(), tags.end(), key) != tags.end();
}

}  // namespace

bool Battle::skillCondHolds(const SkillCond& cond, const SkillCtx& ctx) const {
  using Kind = SkillCond::Kind;
  // "who": "foe" judges the other fighter, everything else the owner.
  const int uid = cond.foe ? ctx.foe : ctx.unit;
  if (uid < 0 || uid >= (int)units.size()) return false;
  const Unit& u = units[(size_t)uid];
  Cell cell = cond.foe ? ctx.foeCell : ctx.cell;
  if (cell == kNoCell) cell = u.cell;  // kNoCell = where it really stands (a unit off the board has no square)
  const int weapon = cond.foe ? ctx.foeWeapon : ctx.weapon;

  switch (cond.kind) {
    case Kind::Hp:
      // hp * 100 <op> percent * maxhp: whole numbers only, no rounding questions.
      return cmp(cond.op, (long long)u.hp * 100, (long long)cond.value * maxHp(u));
    case Kind::Near: {
      if (cell == kNoCell) return false;  // off the board nothing is near
      const int team = teamOf(u);
      int count = 0;
      for (const Unit& o : units) {
        if (o.id == u.id || !o.alive || o.cell == kNoCell) continue;
        if (grid.distance(cell, o.cell) > cond.within) continue;
        if (cond.enemy ? (isEnemy(u, o) && unitVisibleTo(team, o)) : teamOf(o) == team) {
          if (cond.key.empty() || unitTypeMatchesKey(typeOf(o), cond.key)) ++count;
        }
      }
      return cmp(cond.op, count, cond.value);
    }
    case Kind::Weapon:
      return weapon >= 0 && (db->weapons[(size_t)weapon].id == cond.key || db->weapons[(size_t)weapon].group == cond.key);
    case Kind::Terrain: {
      if (cell == kNoCell) return false;
      const TerrainDef& t = db->terrains[(size_t)cells[(size_t)cell].terrain];
      return t.id == cond.key || hasKey(t.tags, cond.key);
    }
    case Kind::Structure: {
      const int si = cell == kNoCell ? -1 : structureIndexAt(cell);
      if (cond.key.empty()) return (si >= 0) == cond.flag;
      if (si < 0) return false;
      const StructureDef& s = db->structures[(size_t)structures[(size_t)si].type];
      return s.id == cond.key || hasKey(s.tags, cond.key);
    }
    case Kind::Unit:
      return unitTypeMatchesKey(typeOf(u), cond.key);
    case Kind::Carrying:
      return (u.carrying >= 0) == cond.flag;
    case Kind::Round:
      return cmp(cond.op, round, cond.value);
    case Kind::Initiating:
      return ctx.combat && ctx.initiating == cond.flag;
    case Kind::Distance: {
      if (!ctx.combat || ctx.foe < 0 || ctx.unit < 0 || ctx.unit >= (int)units.size()) return false;
      const Unit& a = units[(size_t)ctx.unit];
      const Unit& b = units[(size_t)ctx.foe];
      const Cell ca = ctx.cell == kNoCell ? a.cell : ctx.cell, cb = ctx.foeCell == kNoCell ? b.cell : ctx.foeCell;
      if (ca == kNoCell || cb == kNoCell) return false;
      return cmp(cond.op, grid.distance(ca, cb), cond.value);
    }
    case Kind::Formula: {
      if (!formulas || ctx.unit < 0 || ctx.unit >= (int)units.size()) return false;
      // a = the owner, b = the foe (nil without one), the usual seven variables (docs/SRPG.md "Formulas").
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
      return formulas->evalExpr(cond.expr, fc, 0) != 0;
    }
  }
  return false;
}

}  // namespace ote
