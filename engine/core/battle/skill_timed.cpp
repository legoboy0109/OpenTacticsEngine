// Timed skill blocks (docs/SRPG.md "Skills": turnStart, defeat, afterCombat; effects heal, hurt, actAgain).
// Unlike the combat blocks these change the battle, so they run only from the hooks in srpg_hooks.cpp.
#include <algorithm>

#include "core/battle/combat_mods.h"
#include "core/script/formulas.h"

namespace ote {

namespace {

// The last weapon `unit` struck `foe` with in this action's events; -1 if it made no strike.
int weaponUsed(const std::vector<Event>& events, int unit, int foe) {
  for (size_t i = events.size(); i-- > 0;)
    if (events[i].type == EventType::Attack && events[i].a == unit && events[i].b == foe) return events[i].c;
  return -1;
}

}  // namespace

void Battle::runTimedBlock(int owner, int foe, int skill, const SkillBlock& block, const SkillCtx& ctx,
                           std::vector<Event>& events) {
  // An activation is rolled only now (its conditions held), by the same rule as every chance.
  if (block.hasChance && !rollActivation(rng, skillChance(block, ctx))) return;
  const Unit& me = units[(size_t)owner];
  std::vector<Event> fx;  // what the effects did; the SkillActivated event goes in front of it
  std::vector<int> deaths;  // units a "hurt" with "canKill" brought to 0 HP: they fall after the events so far (killUnit pushes UnitDied)
  bool changed = false;
  // The amount of a heal / hurt for one receiver: a number, a percent of its maximum (at least 1), or a formula (a = the receiver).
  auto amountOf = [&](int target, const SkillEffect& e) -> int {
    const Unit& t = units[(size_t)target];
    if (e.percent) return std::max(1, (int)((long long)maxHp(t) * e.value / 100));
    if (e.expr < 0) return e.value;
    FormulaCtx fc;
    fc.sides = &sides;
    fc.battle = this;
    fc.a = &t;
    Props scratch;
    if (t.cell != kNoCell) fc.ta = &numbersAt(t.cell, scratch);
    const double v = formulas ? formulas->evalExpr(e.expr, fc, 0) : 0;
    return !(v > 0) ? 0 : v >= 1000000 ? 1000000 : (int)v;
  };
  auto heal = [&](int target, int amount) {
    Unit& t = units[(size_t)target];
    const int gain = std::min(amount, maxHp(t) - t.hp);
    if (gain <= 0) return;
    t.hp += gain;
    Event e;
    e.type = EventType::UnitHealed;
    e.a = target;
    e.b = gain;
    fx.push_back(e);
    changed = true;
  };
  auto hurt = [&](int target, int amount, bool kill) {
    Unit& t = units[(size_t)target];
    const int loss = std::min(amount, kill ? t.hp : t.hp - 1);  // never below 1, unless the effect may defeat
    if (loss <= 0) return;
    t.hp -= loss;
    Event e;
    e.type = EventType::UnitHurt;
    e.a = target;
    e.b = loss;
    e.c = owner;
    fx.push_back(e);
    changed = true;
    if (t.hp <= 0) deaths.push_back(target);
  };
  // Units of the owner's team (allies) or the enemies its team sees, within `within` steps of the owner.
  auto near = [&](bool enemies, int within, std::vector<int>& out) {
    out.clear();
    if (me.cell == kNoCell) return;
    for (const Unit& o : units) {
      if (o.id == owner || !o.alive || o.cell == kNoCell || grid.distance(me.cell, o.cell) > within) continue;
      if (enemies ? (isEnemy(me, o) && unitVisibleTo(teamOf(me), o)) : teamOf(o) == teamOf(me)) out.push_back(o.id);
    }
  };
  std::vector<int> receivers;
  for (const SkillEffect& e : block.effects) {
    using K = SkillEffect::Kind;
    if (e.kind == K::Heal) {
      if (e.to == SkillEffect::To::Allies) {
        near(false, e.within, receivers);
        for (int r : receivers) heal(r, amountOf(r, e));
      } else {
        heal(owner, amountOf(owner, e));
      }
    } else if (e.kind == K::Hurt) {
      if (e.to == SkillEffect::To::Foe) {
        if (foe >= 0 && units[(size_t)foe].alive) hurt(foe, amountOf(foe, e), e.kill);
      } else if (e.to == SkillEffect::To::Self) {
        if (units[(size_t)owner].alive) hurt(owner, amountOf(owner, e), e.kill);  // a condition's damage over time
      } else {
        near(true, e.within, receivers);
        for (int r : receivers) hurt(r, amountOf(r, e), e.kill);
      }
    } else if (e.kind == K::Cure) {
      if (units[(size_t)owner].alive && cureConditions(owner, e.conds, e.cureAll, kConditionCured, fx) > 0) changed = true;
    } else if (e.kind == K::ActAgain) {
      // Only on its own side's turn, once a turn. Its flags are cleared now; endOrder (which a combat ends with)
      // is told not to end the turn again.
      Unit& u = units[(size_t)owner];
      if (u.refreshed || !u.alive || u.side != activeSide) continue;
      u.moved = u.done = u.acted = false;
      u.moveSpent = 0;
      u.refreshed = true;
      actAgainPending_ = owner;
      changed = true;
    }
  }
  if (!block.hasChance && !changed) return;
  Event a;
  a.type = EventType::SkillActivated;
  a.a = owner;
  a.b = skill;
  a.c = foe;
  events.push_back(a);
  events.insert(events.end(), fx.begin(), fx.end());
  // Defeated by the effect itself: a normal death (UnitDied, summons, rescue). The story and the win checks wait until the
  // blocks of the whole turn start have run (conditionsSettleDeaths), because they may add units or end the battle.
  for (int id : deaths) {
    killUnit(id, events);
    condDeaths_.push_back(id);
  }
}

// After the unit used a staff (docs/SRPG.md "A staff used"): its "staffUse" blocks, in skill-list order, judged with the staff's group
// (the "weapon" condition tests it). They run after the use is spent and the order has ended, like the blocks after a combat.
void Battle::skillsStaffUse(int unit, const std::string& group, std::vector<Event>& events) {
  if (db->skills.empty()) return;
  Unit& u = units[(size_t)unit];
  if (!u.alive || u.cell == kNoCell) return;
  std::vector<int> list;
  skillsOf(u, -1, list);
  SkillCtx ctx;
  ctx.unit = unit;
  ctx.weapon = restingWeapon(u);
  ctx.staffGroup = &group;
  for (int s : list)
    for (const SkillBlock& b : db->skills[(size_t)s].blocks)
      if (units[(size_t)unit].alive && b.when == SkillWhen::StaffUse && skillBlockHolds(b, ctx)) runTimedBlock(unit, -1, s, b, ctx, events);
}

// The start of the active side's turn: its units in id order, each one's blocks in skill-list order.
void Battle::skillsTurnStart(std::vector<Event>& events) {
  if (db->skills.empty()) return;
  std::vector<int> list;
  for (size_t i = 0; i < units.size(); ++i) {
    const Unit& u = units[i];
    if (!u.alive || u.cell == kNoCell || u.side != activeSide) continue;
    skillsOf(u, -1, list);
    SkillCtx ctx;
    ctx.unit = u.id;
    ctx.weapon = restingWeapon(u);
    for (int s : list)
      for (const SkillBlock& b : db->skills[(size_t)s].blocks)
        if (units[i].alive && b.when == SkillWhen::TurnStart && skillBlockHolds(b, ctx)) runTimedBlock(u.id, -1, s, b, ctx, events);
  }
}

// After a combat's experience: the attacker's blocks, then the defender's, each if it is still on the board:
// "defeat" if its foe fell, then "afterCombat". hpA / hpD (the HP before the fight) are not needed here.
void Battle::skillsAfterCombat(int attacker, int defender, int, int, std::vector<Event>& events) {
  if (db->skills.empty()) return;
  std::vector<int> list;
  for (int side = 0; side < 2; ++side) {
    const int owner = side == 0 ? attacker : defender;
    const int foe = side == 0 ? defender : attacker;
    if (!units[(size_t)owner].alive || units[(size_t)owner].cell == kNoCell) continue;
    // The weapons in use: what each really struck with (the events are this action's); else what it would use.
    // No strike was made: the attacker's resting weapon / the defender's answering weapon, or none.
    int own = weaponUsed(events, owner, foe), other = weaponUsed(events, foe, owner);
    if (own < 0) own = side == 0 ? restingWeapon(units[(size_t)owner]) : -2;
    if (other < 0) other = -2;
    SkillCtx ctx;
    ctx.unit = owner;
    ctx.weapon = own;
    ctx.foe = foe;
    ctx.foeWeapon = other;
    ctx.combat = true;
    ctx.initiating = side == 0;
    const bool foeFell = !units[(size_t)foe].alive;
    for (int pass = 0; pass < 2; ++pass) {  // defeat blocks first, then afterCombat
      const SkillWhen when = pass == 0 ? SkillWhen::Defeat : SkillWhen::AfterCombat;
      if (pass == 0 && !foeFell) continue;
      skillsOf(units[(size_t)owner], own >= 0 ? own : -2, list);
      for (int s : list)
        for (const SkillBlock& b : db->skills[(size_t)s].blocks)
          if (b.when == when && skillBlockHolds(b, ctx)) runTimedBlock(owner, foe, s, b, ctx, events);
    }
  }
  if (actAgainPending_ != attacker) actAgainPending_ = -1;  // only the attacker's order is ended after this
}

}  // namespace ote
