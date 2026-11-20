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
          if (cond.key.empty() || unitHasKey(o, cond.key)) ++count;
        }
      }
      return cmp(cond.op, count, cond.value);
    }
    case Kind::Weapon:
      if (!cond.foe && ctx.staffGroup) return *ctx.staffGroup == cond.key;  // after a staff: its group
      return weapon >= 0 && (db->weapons[(size_t)weapon].id == cond.key || db->weapons[(size_t)weapon].group == cond.key);
    case Kind::Terrain: {
      if (cell == kNoCell) return false;
      const TerrainDef& t = db->terrains[(size_t)cells[(size_t)cell].terrain];
      if (cond.keys.empty()) return t.id == cond.key || hasKey(t.tags, cond.key);
      for (const std::string& key : cond.keys)  // the list form: any one
        if (t.id == key || hasKey(t.tags, key)) return true;
      return false;
    }
    case Kind::Structure: {
      const int si = cell == kNoCell ? -1 : structureIndexAt(cell);
      if (cond.key.empty()) return (si >= 0) == cond.flag;
      if (si < 0) return false;
      const StructureDef& s = db->structures[(size_t)structures[(size_t)si].type];
      return s.id == cond.key || hasKey(s.tags, cond.key);
    }
    case Kind::Unit:
      return unitHasKey(u, cond.key);
    case Kind::Counterattack:
      return ctx.counters >= 0 && (ctx.counters == 1) == cond.flag;
    case Kind::Carries: {
      const int item = db->findItem(cond.key);
      for (const ItemStack& s : u.items)
        if (s.item == item) return true;
      return false;
    }
    case Kind::Inventory: {
      const int n = (int)u.items.size();
      if (cond.value < 0) return (n >= db->rules.items.inventorySize) == cond.flag;  // full / notFull
      return cmp(cond.op, n, cond.value);
    }
    case Kind::WeaponKind:
      return weapon >= 0 && db->weapons[(size_t)weapon].damageKind == cond.key;
    case Kind::DamageType:
      return weapon >= 0 && db->weapons[(size_t)weapon].damageType >= 0 &&
             db->damageTypes.list[(size_t)db->weapons[(size_t)weapon].damageType].id == cond.key;
    case Kind::Turn:
      switch (cond.turnMode) {
        case SkillCond::TurnMode::Odd: return round % 2 == 1;
        case SkillCond::TurnMode::Even: return round % 2 == 0;
        case SkillCond::TurnMode::Range: return round >= cond.turnA && round <= cond.turnB;
        case SkillCond::TurnMode::Every: return cond.turnA > 0 && round % cond.turnA == cond.turnB;
      }
      return false;
    case Kind::Strike:
      return ctx.strikeNo > 0 && cmp(cond.op, ctx.strikeNo, cond.value);
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
    case Kind::Condition:
      return hasCondition(u, db->findCondition(cond.key));
    case Kind::Marked:
      return markSet(uid, cond.key) == cond.flag;
    case Kind::Formula:
      return skillExpr(cond.expr, ctx, 0) != 0;
  }
  return false;
}

}  // namespace ote
