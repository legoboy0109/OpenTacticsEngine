// Downed units and revive (contract: docs/SRPG.md "Downed units and revive"). With game.json rules.downed a unit brought to 0 HP is downed where it
// stands (killUnit in combat.cpp): alive == false and hp 0 like any defeated unit, so every rule that asks `alive` (turns, targets, counters, auras,
// win checks, story "alive" / "dead") treats it as defeated, but it keeps its square in `occupant`, so nobody can stand there. Only a revive acts on
// it. A game without the rule never reaches anything here.
#include <algorithm>
#include <cmath>

#include "core/battle/battle.h"
#include "core/script/formulas.h"

namespace ote {

bool Battle::occupantSeen(int team, int occ) const {
  if (occ < 0 || occ >= (int)units.size()) return false;
  return units[(size_t)occ].downed || unitVisibleTo(team, occ);
}

bool Battle::revivable(const Unit& t) const {
  return t.downed && !t.alive && grid.valid(t.cell) && !sides[(size_t)t.side].defeated;
}

int Battle::reviveHp(const Unit& t, const ReviveAmount& r, int user) const {
  const int max = maxHp(t);
  int hp = r.fixed;
  if (r.percent > 0) hp = max * r.percent / 100;
  if (r.expr >= 0) {
    FormulaCtx ctx;
    ctx.sides = &sides;
    ctx.battle = this;
    ctx.a = user >= 0 ? &units[(size_t)user] : &t;
    ctx.b = &t;
    const double v = formulas ? formulas->evalExpr(r.expr, ctx, 0) : 0;
    hp = (int)std::max(0.0, std::min(1000000.0, std::floor(v)));
  }
  return std::max(1, std::min(hp, max));
}

int Battle::abilityReviveHp(const AbilityDef& d, int user, const Unit& t) const {
  // The heal fields (a number, "full", a percent, a formula) read the unit at 0 HP; only the bounds differ from a heal.
  return std::max(1, std::min(abilityHealAmount(d, user, t.id), maxHp(t)));
}

bool Battle::fallCaught(int unit, std::vector<Event>& events) {
  Unit& u = units[(size_t)unit];
  if (!downedGame() || u.conditions.empty()) return false;
  int cond = -1;
  for (const ConditionState& cs : u.conditions)
    if (db->conditions[(size_t)cs.condition].autoRevive.on()) {
      cond = cs.condition;  // the first one applied, if it has several
      break;
    }
  if (cond < 0) return false;
  // It never lies down: the condition is used up and it stands with the condition's HP. Its other conditions stay (it did not fall).
  cureConditions(unit, {cond}, false, kConditionRevived, events);
  u.hp = 0;
  u.hp = reviveHp(u, db->conditions[(size_t)cond].autoRevive, -1);
  Event e;
  e.type = EventType::UnitRevived;
  e.a = unit;
  e.b = u.hp;
  e.c = -1;
  e.d = cond;
  e.flags = kAuto;
  events.push_back(e);
  return true;
}

void Battle::reviveUnit(int unit, int hp, int by, int what, int flags, std::vector<Event>& events) {
  Unit& u = units[(size_t)unit];
  if (!revivable(u)) return;
  markSight();  // its team sees from its square again (events pushed from here on are stamped with the new sight)
  u.alive = true;
  u.downed = false;
  u.downedTurns = 0;
  u.hp = std::max(1, std::min(hp, maxHp(u)));
  // It does not get back the turn it lost: revived during its own turn it has finished it; a speed game's charge starts over.
  if (!speedGame() && u.side == activeSide) u.moved = u.done = true;
  if (speedGame()) {
    u.ct = 0;
    u.turnActed = false;
  }
  Event e;
  e.type = EventType::UnitRevived;
  e.a = unit;
  e.b = u.hp;
  e.c = by;
  e.d = what;
  e.flags = flags;
  events.push_back(e);
}

void Battle::removeDownedBody(int unit, std::vector<Event>& events) {
  Unit& u = units[(size_t)unit];
  if (!u.downed) return;
  Event e;
  e.type = EventType::UnitRemoved;
  e.a = unit;
  e.b = u.cell;
  e.flags = kDowned;
  events.push_back(e);
  markSight();
  if (grid.valid(u.cell) && occupant[(size_t)u.cell] == unit) occupant[(size_t)u.cell] = -1;
  u.cell = kNoCell;
  u.downed = false;
  u.downedTurns = 0;
}

void Battle::downedTurnStart(std::vector<Event>& events) {
  if (!downedGame() || db->rules.downed.removeAfter <= 0) return;
  for (size_t i = 0; i < units.size(); ++i) {  // unit-id order
    Unit& u = units[i];
    if (!u.downed || u.side != activeSide) continue;
    if (++u.downedTurns >= db->rules.downed.removeAfter) removeDownedBody((int)i, events);
  }
}

void Battle::removeDownedOfSide(int side, std::vector<Event>& events) {
  if (!downedGame()) return;
  for (size_t i = 0; i < units.size(); ++i)
    if (units[i].downed && units[i].side == side) removeDownedBody((int)i, events);
}

}  // namespace ote
