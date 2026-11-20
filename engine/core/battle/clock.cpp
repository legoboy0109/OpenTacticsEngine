// The speed turn order's clock (docs/TURN_ORDER.md "The clock"). Units act one at a time: every tick each unit gains its speed in charge; the unit
// that reaches the threshold has the turn (ties: more charge, then more speed, then the lower unit id), and `activeSide` becomes its side so the
// rules that ask "whose turn is it" keep working. Pending charged actions run on the same clock and go off before any unit that fills at the same
// tick (charged_actions.cpp). Nothing here uses randomness.
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

int Battle::speedOf(const Unit& u) const {
  if (!speedGame() || !u.alive || u.cell == kNoCell) return 0;
  if (!u.conditions.empty() && conditionBlock(u, ConditionBlock::Act) >= 0) return 0;  // stopped: its charge does not build
  return std::max(0, stat(u, db->rules.speed.stat));
}

int Battle::ctCostOf(const Unit& u) const {
  const SpeedRules& sp = db->rules.speed;
  if (u.moved && u.turnActed) return sp.costBoth;
  return u.moved || u.turnActed ? sp.costOne : sp.costNone;
}

void Battle::speedStart(std::vector<Event>& events) {
  activeUnit = -1;
  clockTick = 0;
  turnNo = 0;
  chargeSerial = 0;
  pending.clear();
  beginRound(events);
  if (over) return;
  runClock(events);
}

// Who is ahead of whom among units that are ready: more charge, then the higher speed stat, then the lower unit id. A rule with no randomness.
static bool readyBefore(const Battle& b, const Unit& x, const Unit& y) {
  if (x.ct != y.ct) return x.ct > y.ct;
  const int sx = b.stat(x, b.db->rules.speed.stat), sy = b.stat(y, b.db->rules.speed.stat);
  if (sx != sy) return sx > sy;
  return x.id < y.id;
}

void Battle::runClock(std::vector<Event>& events) {
  const SpeedRules& sp = db->rules.speed;
  activeUnit = -1;
  int idle = 0;  // ticks in a row in which nothing gained any charge
  while (!over) {
    dropStaleCharges(events);
    const int p = readyCharge();
    if (p >= 0) {  // a full charged action goes first, before any unit that fills at the same tick
      resolveCharge(p, events);
      continue;
    }
    int pick = -1;
    for (const Unit& u : units) {
      if (!inClock(u) || u.ct < sp.threshold) continue;
      if (pick < 0 || readyBefore(*this, u, units[(size_t)pick])) pick = u.id;
    }
    if (pick >= 0) {
      beginUnitTurn(pick, events);
      if (over) return;
      const Unit& u = units[(size_t)pick];
      if (u.alive && inClock(u) && !unitSpent(u)) return;  // its turn: the person or the computer opponent plays it
      closeUnitTurn(events);  // nothing left to do (a condition took the turn, it fell): on to the next
      continue;
    }
    bool gained = false;
    for (Unit& u : units) {
      const int g = speedOf(u);
      if (g > 0) {
        u.ct += g;
        gained = true;
      }
    }
    for (PendingAction& pa : pending) {
      pa.counter += pa.speed;
      gained = true;
    }
    if (++clockTick >= sp.roundTicks) {
      clockTick = 0;
      ++round;
      beginRound(events);
      if (!over) settle(events);
    }
    idle = gained ? 0 : idle + 1;
    if (idle > 20000 && !over) endBattle(-1, events);  // every unit is stopped for good: nobody can ever act again
  }
}

void Battle::beginUnitTurn(int id, std::vector<Event>& events) {
  const bool newSide = turnNo == 0 || activeSide != units[(size_t)id].side;
  activeUnit = id;
  activeSide = units[(size_t)id].side;
  ++turnNo;
  refreshUnitForTurn(units[(size_t)id]);
  if (newSide) {  // the banner, and the music of the side's phase: only when the turn passes to another side
    Event t;
    t.type = EventType::TurnStarted;
    t.a = activeSide;
    t.b = round;
    events.push_back(t);
  }
  Event e;
  e.type = EventType::UnitTurnBegan;
  e.a = id;
  e.b = round;
  e.c = units[(size_t)id].ct;
  events.push_back(e);
  // The per-unit start-of-turn effects of beginTurn, for this unit alone (every loop asks ownsTurn): the per-side ones ran at the round's start.
  resupplyTurn(events);
  burnFuel(events);
  if (over) return;
  repairTurn(events);
  srpgTurnStart(events);
  if (over) return;
  aiTurnStart();  // docs/AI.md: after healing and damage, so the retreat check sees this turn's hit points
}

void Battle::closeUnitTurn(std::vector<Event>& events) {
  if (activeUnit < 0) return;
  const int id = activeUnit;
  srpgTurnEnd(events);  // what ending a turn earns (supports) and the conditions that run out, while the unit still has the turn
  Unit& u = units[(size_t)id];
  if (inClock(u)) u.ct -= ctCostOf(u);
  refreshUnitForTurn(u);  // an idle unit shows no "moved / done" marks
  activeUnit = -1;
}

void Battle::endUnitTurn(std::vector<Event>& events) {
  closeUnitTurn(events);
  if (!over) runClock(events);
}

void Battle::speedAfterAction(std::vector<Event>& events) {
  if (over) return;
  dropStaleCharges(events);
  if (activeUnit < 0) {  // (a story run that ended the turn itself)
    runClock(events);
    return;
  }
  const Unit& u = units[(size_t)activeUnit];
  if (!u.alive || !inClock(u) || unitSpent(u)) endUnitTurn(events);
  else activeSide = u.side;  // a story command may have changed its side
}

}  // namespace ote
