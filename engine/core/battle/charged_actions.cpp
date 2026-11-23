// Charged actions (docs/TURN_ORDER.md "Charged actions"). An ability or weapon with a "chargeSpeed" is not carried out when it is chosen: the unit
// DECLARES it (its target is fixed now, its turn ends), and it goes off when its own counter, gaining chargeSpeed per tick, reaches the clock's
// threshold. It follows the unit that was aimed at, or hits the square that was aimed at, by the definition's "chargeTargets". It is judged again
// when it goes off: out of range or nothing there = it fizzles.
//
// Why it ends: a declared action leaves `pending` when it goes off, fizzles, is cancelled (the caster fell, got an "interruptedBy" condition, left
// the board, or gave it up) and nothing puts one back, so the clock always empties it.
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

int Battle::pendingOf(int unit) const {
  for (size_t i = 0; i < pending.size(); ++i)
    if (pending[i].unit == unit) return (int)i;
  return -1;
}

PendingAction Battle::chargeFor(int unit, bool weapon, int index, int target, Cell cell, int slot) const {
  PendingAction p;
  p.unit = unit;
  p.weapon = weapon;
  p.index = index;
  p.slot = slot;
  ChargeDef::Aim aim;
  if (weapon) {
    const WeaponDef& w = db->weapons[(size_t)index];
    p.speed = w.charge.speed;
    aim = w.charge.aim != ChargeDef::Aim::Default ? w.charge.aim : w.area.on() ? ChargeDef::Aim::Cell : ChargeDef::Aim::Unit;
  } else {
    const AbilityDef& d = db->abilities[(size_t)index];
    p.speed = d.charge.speed;
    aim = d.charge.aim != ChargeDef::Aim::Default ? d.charge.aim : d.target == AbilityTarget::Cell ? ChargeDef::Aim::Cell : ChargeDef::Aim::Unit;
    if (d.target == AbilityTarget::Self) target = unit;  // it follows the caster, wherever the caster goes
  }
  Cell at = cell;
  if (at == kNoCell && target >= 0 && (size_t)target < units.size()) at = units[(size_t)target].cell;
  p.follow = aim == ChargeDef::Aim::Unit && target >= 0;
  p.target = p.follow ? target : -1;
  p.cell = at;
  return p;
}

Cell Battle::chargeAimCell(const PendingAction& p) const {
  if (p.follow && p.target >= 0 && (size_t)p.target < units.size() && units[(size_t)p.target].alive) return units[(size_t)p.target].cell;
  return p.cell;
}

// What a unit that is charging may do with its turn: move, wait, handle its things, or give the charge up. And a new declaration's own rules.
bool Battle::chargeAllowed(const Action& action, std::string* why) const {
  auto refuse = [&](const char* reason) {
    if (why) *why = reason;
    return false;
  };
  if (action.type == ActionType::CancelCharge) return charging(action.unit) ? true : refuse("that unit has no charged action to give up");
  if (charging(action.unit)) {
    switch (action.type) {
      case ActionType::Move: case ActionType::Wait: case ActionType::Equip: case ActionType::GiveItem: case ActionType::TakeItem:
      case ActionType::StoreItem: case ActionType::FetchItem: case ActionType::BuyItem: case ActionType::DiscardItem: case ActionType::SwapItems:
      case ActionType::SwitchPair: case ActionType::GiveUnit: case ActionType::TakeUnit: case ActionType::Support:
        break;
      default: return refuse("that unit is charging an action: it may only move, or give the charge up");
    }
  }
  if (action.type == ActionType::Attack && weaponCharges(action.weapon)) {
    const Unit& u = units[(size_t)action.unit];
    if (mapWeaponUsedBy(u, u.cell, action.weapon) >= 0) return refuse("a charged weapon cannot be fired from a structure");
  }
  return true;
}

void Battle::declareCharge(const Action& action, std::vector<Event>& events) {
  Unit& u = units[(size_t)action.unit];
  const bool weapon = action.type == ActionType::Attack;
  PendingAction p;
  if (weapon) {
    // An area weapon aims at a square (the target is the unit standing there, if any); a single-target weapon at a unit.
    const Cell aim = isAreaWeapon(action.weapon) ? aimedCell(action) : kNoCell;
    p = chargeFor(action.unit, true, action.weapon, action.target, aim, -1);
  } else {
    p = chargeFor(action.unit, false, action.ability, action.target, action.cell, action.slot);
  }
  p.serial = ++chargeSerial;
  const Cell at = chargeAimCell(p);
  pending.push_back(p);
  if (weapon) {
    if (facingGame() && at != kNoCell) faceToward(action.unit, at, events);  // rules.facing: it turns to what it aims at
  } else {
    noteAbilityUse(action.unit, action.ability);  // the use, the cooldown and the item are paid now, whether or not it goes off
    spendAbilityItem(action.unit, action.ability, events);
    payAbilityPrice(action.unit, action.ability, events);  // "spend" too (docs/SRPG.md "Unit resources")
  }
  Event e;
  e.type = EventType::ActionCharging;
  e.a = action.unit;
  e.b = weapon ? 1 : 0;
  e.c = p.index;
  e.d = at;
  e.data = {p.follow ? p.target : -1, p.speed};
  events.push_back(e);
  u.turnActed = true;
  u.done = true;  // the declaration ends the turn (the clock charges it as a turn that acted)
}

void Battle::cancelCharge(int index, int reason, std::vector<Event>& events) {
  const PendingAction p = pending[(size_t)index];
  pending.erase(pending.begin() + index);
  Event e;
  e.type = EventType::ChargeCancelled;
  e.a = p.unit;
  e.b = p.weapon ? 1 : 0;
  e.c = p.index;
  e.d = reason;
  events.push_back(e);
}

void Battle::chargeUnitGone(int unit, std::vector<Event>& events) {
  for (size_t i = pending.size(); i-- > 0;)
    if (pending[i].unit == unit) cancelCharge((int)i, kChargeDefeated, events);
}

void Battle::chargeCondition(int unit, int condition, std::vector<Event>& events) {
  for (size_t i = pending.size(); i-- > 0;) {
    const PendingAction& p = pending[i];
    if (p.unit != unit) continue;
    const std::vector<int>& list = p.weapon ? db->weapons[(size_t)p.index].charge.interrupt : db->abilities[(size_t)p.index].charge.interrupt;
    if (std::find(list.begin(), list.end(), condition) != list.end()) cancelCharge((int)i, kChargeInterrupted, events);
  }
}

void Battle::dropStaleCharges(std::vector<Event>& events) {
  for (size_t i = pending.size(); i-- > 0;) {
    const int c = pending[i].unit;
    if (c < 0 || (size_t)c >= units.size() || !inClock(units[(size_t)c])) cancelCharge((int)i, kChargeGone, events);
  }
}

// The pending action that goes first: the fullest counter, then the faster one, then the older.
int Battle::readyCharge() const {
  const int threshold = db->rules.speed.threshold;
  int best = -1;
  for (size_t i = 0; i < pending.size(); ++i) {
    const PendingAction& p = pending[i];
    if (p.counter < threshold) continue;
    if (best >= 0) {
      const PendingAction& b = pending[(size_t)best];
      if (p.counter != b.counter ? p.counter < b.counter : p.speed != b.speed ? p.speed < b.speed : p.serial > b.serial) continue;
    }
    best = (int)i;
  }
  return best;
}

void Battle::resolveCharge(int index, std::vector<Event>& events) {
  const PendingAction p = pending[(size_t)index];
  pending.erase(pending.begin() + index);
  const Unit& caster = units[(size_t)p.unit];
  Event gone;  // how it ends when it does not go off
  gone.type = EventType::ChargeCancelled;
  gone.a = p.unit;
  gone.b = p.weapon ? 1 : 0;
  gone.c = p.index;
  if (!inClock(caster)) {
    gone.d = kChargeGone;
    events.push_back(gone);
    return;
  }
  // Where it lands now: the followed unit's square, or the square it was aimed at.
  int target = -1;
  Cell aim = p.cell;
  if (p.follow) {
    if (!units[(size_t)p.target].alive) {
      gone.d = kChargeFizzled;  // the unit it followed is gone
      events.push_back(gone);
      return;
    }
    target = p.target;
    aim = units[(size_t)target].cell;
  } else if (aim != kNoCell) {
    target = unitAt(aim);
  }
  Event used;
  used.type = EventType::ChargeResolved;
  used.a = p.unit;
  used.b = p.weapon ? 1 : 0;
  used.c = p.index;
  used.d = aim;
  if (p.weapon) {
    const bool area = isAreaWeapon(p.index);
    bool ok = false;
    if (area) {
      AreaForecast f;
      ok = aim != kNoCell && areaForecast(p.unit, p.index, caster.cell, aim, f);
    } else {
      Forecast f;
      ok = target >= 0 && forecast(p.unit, target, p.index, caster.cell, f);
    }
    if (!ok) {  // out of reach now, or nobody to hit
      gone.d = kChargeFizzled;
      events.push_back(gone);
      return;
    }
    events.push_back(used);
    Action a = Action::attack(p.unit, target, p.index);
    a.cell = area ? aim : kNoCell;
    applyAttack(a, events);
  } else {
    const AbilityDef& d = db->abilities[(size_t)p.index];
    Cell cell = kNoCell;
    if (d.target == AbilityTarget::Self) target = p.unit;
    else if (d.target == AbilityTarget::Cell) {
      cell = p.cell;
      target = -1;
    }
    bool ok = (d.target == AbilityTarget::Cell || (target >= 0 && units[(size_t)target].alive)) &&
              abilityTargetOk(caster, p.index, caster.cell, target, cell, nullptr);
    if (ok && d.effect == AbilityEffect::Steal) {
      std::vector<StealOption> opts;
      stealOptions(p.unit, p.index, caster.cell, target, opts);
      ok = p.slot >= 0 && p.slot < (int)opts.size() && opts[(size_t)p.slot].ok;
    }
    if (!ok) {
      gone.d = kChargeFizzled;
      events.push_back(gone);
      return;
    }
    events.push_back(used);
    Action a = Action::useAbility(p.unit, p.index, target, cell);
    a.slot = p.slot;
    applyAbility(a, events, true);
  }
  // The caster is idle again: whatever the action's ending set on its turn flags does not belong to a turn.
  if (units[(size_t)p.unit].alive) refreshUnitForTurn(units[(size_t)p.unit]);
  settle(events);
}

}  // namespace ote
