// Staves in a battle, the questions (docs/SRPG.md section 8): who may use one, how far it reaches, which units it would
// affect, the menu list and the forecast. Nothing here changes the battle or rolls a die, so menus, the forecast and the
// computer opponent can ask freely. Carrying a use out is staves_apply.cpp.
#include <algorithm>
#include <cmath>

#include "core/battle/battle.h"
#include "core/script/formulas.h"

namespace ote {

namespace {

// A formula's number as a whole number: rounded down, never negative (a heal, a range, experience).
int whole(double v, int top) {
  if (!(v > 0)) return 0;  // also catches NaN
  return v >= top ? top : (int)std::floor(v);
}

const StaffDef* staffDefOf(const Database& db, int item) {
  if (item < 0 || (size_t)item >= db.items.size() || db.items[(size_t)item].kind != ItemKind::Staff) return nullptr;
  return &db.staves[(size_t)db.items[(size_t)item].staff];
}

}  // namespace

// Class, then the character lock, then the rank: the order of canWieldNow, with the same words.
bool Battle::canWieldStaff(const Unit& u, int item, std::string* why) const {
  const StaffDef* s = staffDefOf(*db, item);
  if (!s) {
    if (why) *why = "that is not a staff";
    return false;
  }
  if (!db->canWieldGroup(u.type, s->gear.group, u.character)) {
    if (why) *why = "the unit cannot wield that";
    return false;
  }
  if (!u.conditions.empty()) {  // "silence": a condition that forbids this group or staves at all (docs/SRPG.md section 9)
    int c = conditionForbidsGroup(u, s->gear.group);
    if (c < 0) c = conditionForbidsKind(u, ItemKind::Staff);
    if (c >= 0) {
      if (why) *why = db->conditions[(size_t)c].name + " stops that";
      return false;
    }
  }
  if (!meetsLockFor(u, s->gear, why)) return false;
  return meetsRankFor(u, s->gear, why);
}

double Battle::staffEval(int expr, const Unit& user, const Unit* target, const StaffDef& d, Cell from, double def) const {
  if (!formulas || expr < 0) return def;
  FormulaCtx fc;
  fc.sides = &sides;
  fc.battle = this;
  fc.a = &user;
  fc.b = target;
  fc.w = &d.gear;
  Props scratchA, scratchB;
  const Cell ca = from == kNoCell ? user.cell : from;
  const Cell cb = target ? (target->id == user.id ? ca : target->cell) : kNoCell;
  if (ca != kNoCell) fc.ta = &numbersAt(ca, scratchA);
  if (cb != kNoCell) fc.tb = &numbersAt(cb, scratchB);
  if (ca != kNoCell && cb != kNoCell) fc.dist = grid.distance(ca, cb);
  return formulas->evalExpr(expr, fc, def);
}

void Battle::staffRange(const Unit& u, int item, int& minRange, int& maxRange) const {
  minRange = 1;
  maxRange = 1;
  const StaffDef* s = staffDefOf(*db, item);
  if (!s) return;
  if (s->rangeExpr < 0) {
    minRange = s->minRange;
    maxRange = s->maxRange;
  } else {
    maxRange = whole(staffEval(s->rangeExpr, u, nullptr, *s, kNoCell, 0), kMaxStaffRange);
  }
  // "staffRange" skills (docs/SRPG.md "Staff range bonus"): the number, [min, max] and formula forms all raise the maximum. The one
  // place reach is worked out, so the menu, the player's reach display, the rules and the computer opponent agree.
  const int bonus = staffRangeBonus(u, s->gear.group);
  if (bonus > 0) maxRange = std::min(kMaxStaffRange, maxRange + bonus);
}

int Battle::staffRangeBonus(const Unit& u, const std::string& group) const {
  if (!db->staffRangeSkills) return 0;
  std::vector<const SkillEffect*> fx;
  standingEffects(u, false, fx);
  int sum = 0;
  for (const SkillEffect* e : fx)
    if (e->kind == SkillEffect::Kind::StaffRange && (e->names.empty() || e->names[0] == group)) sum += e->value;
  return sum;
}

int Battle::staffHealAmount(const Unit& user, const StaffDef& d, const StaffEffect& fx, const Unit& t, Cell from) const {
  switch (fx.amount) {
    case StaffEffect::Amount::Fixed: return fx.value;
    case StaffEffect::Amount::Full: return maxHp(t);
    case StaffEffect::Amount::Percent: return std::max(1, maxHp(t) * fx.value / 100);
    case StaffEffect::Amount::Formula: return whole(staffEval(fx.expr, user, &t, d, from, 0), 1000000);
  }
  return 0;
}

// The units the staff would really change. `from` is where the user stands for the question; its own square is `from`
// whenever the user itself is a candidate (a team heal, a self staff).
void Battle::staffAffected(const Unit& user, int item, int slot, int center, Cell from, std::vector<int>& out) const {
  out.clear();
  const StaffDef* sp = staffDefOf(*db, item);
  if (!sp || !grid.valid(from) || sp->picksSquare()) return;
  const StaffDef& d = *sp;
  const int team = teamOf(user);
  auto cellOf = [&](const Unit& t) { return t.id == user.id ? from : t.cell; };
  int minR = 1, maxR = 1;
  staffRange(user, item, minR, maxR);
  const Cell centerCell = center >= 0 && center < (int)units.size() ? cellOf(units[(size_t)center]) : kNoCell;

  for (const Unit& t : units) {  // ascending id
    if (!t.alive || t.cell == kNoCell) continue;
    const bool mine = teamOf(t) == team;
    bool kind = false;
    switch (d.target) {
      case StaffTarget::Ally: kind = mine && t.id != user.id; break;
      case StaffTarget::Self: kind = t.id == user.id; break;
      case StaffTarget::Team: kind = mine; break;
      case StaffTarget::Foe: kind = !mine && unitVisibleTo(team, t); break;
      case StaffTarget::Any: kind = t.id != user.id && unitVisibleTo(team, t); break;
    }
    if (!kind) continue;
    switch (d.area) {
      case StaffArea::Single:
        if (t.id != center) continue;
        break;
      case StaffArea::Radius:
        if (centerCell == kNoCell || grid.distance(centerCell, cellOf(t)) > d.radius) continue;
        break;
      case StaffArea::All: {
        if (d.target == StaffTarget::Self) break;
        const int dist = grid.distance(from, cellOf(t));
        if (dist < minR || dist > maxR) continue;
        break;
      }
    }
    // Conditions ("who": "foe" = this unit), judged like an ability's.
    SkillCtx ctx;
    ctx.unit = user.id;
    ctx.cell = from;
    ctx.weapon = restingWeapon(user);
    ctx.foe = t.id;
    bool holds = true;
    for (const SkillCond& cond : d.conds) holds = holds && skillCondHolds(cond, ctx);
    if (!holds) continue;
    // "Would change": some effect does something to this unit (a heal needs a hurt one, a pull a free square, ...).
    bool changes = false;
    for (const StaffEffect& fx : d.effects) changes = changes || !staffEffectWhy(user, d, fx, slot, t, from);
    if (changes) out.push_back(t.id);
  }
}

void Battle::staffTargets(int unit, int slot, Cell from, std::vector<int>& out) const {
  out.clear();
  if (unit < 0 || unit >= (int)units.size() || !grid.valid(from)) return;
  const Unit& u = units[(size_t)unit];
  if (!u.alive || slot < 0 || slot >= (int)u.items.size()) return;
  const int item = u.items[(size_t)slot].item;
  const StaffDef* sp = staffDefOf(*db, item);
  if (!sp || !canWieldStaff(u, item)) return;
  const StaffDef& d = *sp;
  std::vector<int> hit;
  if (d.picksSquare()) return;  // a light picks a square (staffLightCells)
  if (!d.needsPick()) {  // nothing to pick: confirm when it would do something
    staffAffected(u, item, slot, d.target == StaffTarget::Self ? unit : -1, from, hit);
    if (!hit.empty()) out.push_back(unit);
    return;
  }
  int minR = 1, maxR = 1;
  staffRange(u, item, minR, maxR);
  const int team = teamOf(u);
  for (const Unit& t : units) {
    if (!t.alive || t.cell == kNoCell) continue;
    const bool mine = teamOf(t) == team;
    if ((d.target == StaffTarget::Ally && (!mine || t.id == unit)) || (d.target == StaffTarget::Team && !mine) ||
        (d.target == StaffTarget::Foe && (mine || !unitVisibleTo(team, t))) ||
        (d.target == StaffTarget::Any && (t.id == unit || !unitVisibleTo(team, t))))
      continue;
    const int dist = grid.distance(from, t.id == unit ? from : t.cell);
    if (dist < minR || dist > maxR) continue;
    staffAffected(u, item, slot, t.id, from, hit);
    if (!hit.empty()) out.push_back(t.id);
  }
}

void Battle::staffOptions(int unit, Cell from, std::vector<StaffOption>& out) const {
  out.clear();
  if (unit < 0 || unit >= (int)units.size() || !grid.valid(from)) return;
  const Unit& u = units[(size_t)unit];
  for (size_t i = 0; i < u.items.size(); ++i) {
    const int item = u.items[i].item;
    if (!staffDefOf(*db, item)) continue;
    StaffOption o;
    o.slot = (int)i;
    o.item = item;
    o.ok = canWieldStaff(u, item, &o.why);
    if (o.ok) {
      const StaffDef& d = *staffDefOf(*db, item);
      std::vector<int> targets;
      std::vector<Cell> cells;
      if (d.has(StaffEffect::Kind::Unlock)) staffUnlockCells(unit, (int)i, from, cells);
      else if (d.picksSquare()) staffLightCells(unit, (int)i, from, cells);
      else staffTargets(unit, (int)i, from, targets);
      if (targets.empty() && cells.empty()) {
        o.ok = false;
        o.why = !d.picksSquare() || d.has(StaffEffect::Kind::Unlock) ? staffNobodyWhy(d) : !fog ? "this map has no fog of war" : "nothing in reach is hidden";
      }
    }
    out.push_back(o);
  }
}

bool Battle::staffForecast(int unit, int slot, int target, Cell from, StaffForecast& out, Cell dest, int itemSlot) const {
  out = StaffForecast();
  if (unit < 0 || unit >= (int)units.size() || !grid.valid(from)) return false;
  const Unit& u = units[(size_t)unit];
  if (!u.alive || slot < 0 || slot >= (int)u.items.size()) return false;
  const int item = u.items[(size_t)slot].item;
  const StaffDef* sp = staffDefOf(*db, item);
  if (!sp || !canWieldStaff(u, item)) return false;
  const StaffDef& d = *sp;
  out.canMiss = d.canMiss();
  out.user = unit;
  out.slot = slot;
  out.item = item;
  out.usesLeft = u.items[(size_t)slot].uses;
  out.breaks = out.usesLeft == 1;
  staffRange(u, item, out.minRange, out.maxRange);
  if (d.has(StaffEffect::Kind::Unlock)) {  // doors and chests: `target` is a square of the one to open
    std::vector<Cell> legal;
    staffUnlockCells(unit, slot, from, legal);
    if (std::find(legal.begin(), legal.end(), (Cell)target) == legal.end()) return false;
    out.isUnlock = true;
    out.needsPick = false;
    out.unlockCell = (Cell)target;
    out.unlockStructure = structureAt[(size_t)target];
    if (d.canMiss()) out.unlockHit = std::max(0, std::min(100, (int)std::lround(staffEval(d.hitExpr, u, nullptr, d, from, 100))));
    out.xp = d.xpExpr >= 0 ? whole(staffEval(d.xpExpr, u, nullptr, d, from, 0), kMaxStaffXp) : d.xp;
    return true;
  }
  if (d.picksSquare()) {  // a light: `target` is the square
    std::vector<Cell> legal;
    staffLightCells(unit, slot, from, legal);
    if (std::find(legal.begin(), legal.end(), (Cell)target) == legal.end()) return false;
    const StaffEffect& fx = *d.find(StaffEffect::Kind::Light);
    out.isLight = true;
    out.needsPick = false;
    out.lightCell = (Cell)target;
    out.lightRadius = fx.radius;
    out.lightTurns = fx.turns;
    grid.within(out.lightCell, fx.radius, out.area);
    for (Cell c : out.area) out.newSquares += !visibleTo(teamOf(u), c);
    out.xp = d.xpExpr >= 0 ? whole(staffEval(d.xpExpr, u, nullptr, d, from, 0), kMaxStaffXp) : d.xp;
    return true;
  }
  std::vector<int> legal;
  staffTargets(unit, slot, from, legal);
  out.needsPick = d.needsPick();
  out.center = out.needsPick ? target : (d.target == StaffTarget::Self ? unit : -1);
  if (out.needsPick ? std::find(legal.begin(), legal.end(), target) == legal.end() : legal.empty()) return false;
  std::vector<int> hit;
  staffAffected(u, item, slot, out.center, from, hit);
  const StaffEffect* warp = d.find(StaffEffect::Kind::Warp);
  const bool repairs = d.has(StaffEffect::Kind::Repair);
  std::vector<int> mendable;
  if (repairs && out.center >= 0) staffRepairSlots(unit, slot, out.center, mendable);
  out.needsDest = warp && dest == kNoCell;
  out.needsItem = repairs && !mendable.empty() && itemSlot < 0;
  for (int id : hit) {
    const Unit& t = units[(size_t)id];
    StaffHit h;
    h.unit = id;
    h.hpBefore = t.hp;
    h.hpAfter = t.hp;
    for (const StaffEffect& fx : d.effects) {
      if (fx.kind != StaffEffect::Kind::Heal) continue;
      const int give = staffHealAmount(u, d, fx, t, from);
      h.nominal += give;
      h.hpAfter = std::min(maxHp(t), h.hpAfter + give);
    }
    h.amount = h.hpAfter - h.hpBefore;
    if (const StaffEffect* fx = d.find(StaffEffect::Kind::Cure))
      for (const ConditionState& cs : t.conditions)
        if (fx->cureAll || std::find(fx->conds.begin(), fx->conds.end(), cs.condition) != fx->conds.end()) h.cures.push_back(cs.condition);
    if (const StaffEffect* fx = d.find(StaffEffect::Kind::Inflict))
      if (inflictWhy(t, fx->conds[0], fx->turns) == 0) {
        h.inflict = fx->conds[0];
        h.inflictTurns = conditionDuration(fx->conds[0], fx->turns);
      }
    if (d.has(StaffEffect::Kind::Pull)) h.moveTo = staffPullCell(unit, id, from);
    if (d.has(StaffEffect::Kind::Swap)) {
      h.moveTo = from;
      h.userTo = t.cell;
    }
    if (warp && dest != kNoCell) h.moveTo = dest;
    if (repairs && itemSlot >= 0 && itemSlot < (int)t.items.size()) {
      const StaffEffect& fx = *d.find(StaffEffect::Kind::Repair);
      const ItemStack& st = t.items[(size_t)itemSlot];
      const int top = db->items[(size_t)st.item].uses;
      h.repairItem = st.item;
      h.usesBefore = st.uses;
      h.usesAfter = fx.amount == StaffEffect::Amount::Full ? top : std::min(top, st.uses + fx.value);
    }
    if (d.canMiss()) h.hit = std::max(0, std::min(100, (int)std::lround(staffEval(d.hitExpr, u, &t, d, from, 100))));
    out.hits.push_back(h);
  }
  const Unit* centerUnit = out.center >= 0 ? &units[(size_t)out.center] : nullptr;
  out.xp = d.xpExpr >= 0 ? whole(staffEval(d.xpExpr, u, centerUnit, d, from, 0), kMaxStaffXp) : d.xp;
  return true;
}

}  // namespace ote
