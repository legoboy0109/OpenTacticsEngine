// The action-point turn system (docs/SRPG.md "Action points"): what moving and attacking cost, what a unit may still do, and the one question
// "is this unit finished" (unitSpent). The pool is a resource of the side; it is paid with changeResource so the ResourceChanged events
// carry the cost to the player, the save and the story like any other price.
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

int Battle::apPool(int side) const {
  if (!db->rules.ap.on || side < 0 || side >= (int)sides.size()) return 0;
  return sides[(size_t)side].resources[(size_t)db->rules.ap.resource];
}

int Battle::apMoveCost(const Unit& u, int points, int spentBefore) const {
  const ActionPointRules& ap = db->rules.ap;
  if (!ap.on || points <= 0) return 0;
  const UnitTypeDef& t = typeOf(u);
  const int step = t.apStep >= 0 ? t.apStep : ap.stepCost;
  const int cap = t.apSoftCap >= 0 ? t.apSoftCap : ap.softCap;  // -1 = none
  if (cap < 0) return step * points;
  const int spent = std::max(0, spentBefore >= 0 ? spentBefore : u.moveSpent);
  const int plain = std::max(0, std::min(points, cap - spent));  // the points still inside the cap
  const int dear = (step * (100 + ap.softCapPercent) + 99) / 100;  // rounded up: a half AP is a whole one
  return plain * step + (points - plain) * dear;
}

int Battle::apAttackCost(int unit, int weapon) const {
  const ActionPointRules& ap = db->rules.ap;
  if (!ap.on || weapon < 0 || weapon >= (int)db->weapons.size()) return 0;
  (void)unit;
  const WeaponDef& w = db->weapons[(size_t)weapon];
  return (w.apCost >= 0 ? w.apCost : ap.attackCost) + w.area.apExtra;
}

int Battle::apAttacksLeft(const Unit& u) const {
  return db->rules.ap.on ? std::max(0, db->rules.ap.attacksPerTurn - u.apAttacks) : 1;
}

int Battle::apMoveBudget(const Unit& u, int budget, bool limitByPool) const {
  const ActionPointRules& ap = db->rules.ap;
  if (!ap.splitMoves && u.moved) return 0;      // one move per turn, made
  int left = std::max(0, budget - u.moveSpent);  // the Move stat is the most a unit walks per turn over all its moves
  if (!limitByPool) return left;
  const int pool = apPool(u.side);
  int p = 0;
  while (p < left && apMoveCost(u, p + 1) <= pool) ++p;  // the price never falls as points are added, so the first miss ends it
  return p;
}

int Battle::apMovePointsLeft(const Unit& u) const {
  if (!db->rules.ap.on) return 0;
  return apMoveBudget(u, movePoints(u), true);
}

bool Battle::apMayMove(const Unit& u) const {
  return db->rules.ap.on && u.alive && apMovePointsLeft(u) > 0;
}

bool Battle::apMayAttack(const Unit& u) const {
  if (!db->rules.ap.on || !u.alive || apAttacksLeft(u) <= 0) return false;
  std::vector<int> weapons;
  weaponsOf(u, weapons);
  const int pool = apPool(u.side);
  for (int w : weapons) {
    if (u.moved && !db->weapons[(size_t)w].afterMove) continue;  // artillery that already walked
    if (apAttackCost(u.id, w) <= pool) return true;
  }
  return false;
}

bool Battle::unitSpent(const Unit& u) const {
  if (u.done) return true;
  return db->rules.ap.on && !apMayMove(u) && !apMayAttack(u);
}

bool Battle::apAttackAllowed(const Unit& u, int weapon, std::string* why) const {
  if (apAttacksLeft(u) <= 0) {
    if (why) *why = "that unit has no attacks left this turn";
    return false;
  }
  if (apAttackCost(u.id, weapon) > apPool(u.side)) {
    if (why) *why = "not enough AP";
    return false;
  }
  return true;
}

const char* Battle::apMoveRefusal(int unit, Cell cell) const {
  const Unit& u = units[(size_t)unit];
  if (!db->rules.ap.on) return "the unit cannot reach that cell";
  Reach free;
  reachFor(unit, -1, free, false);  // the search without the pool: is it the AP that is short?
  if (free.canStop(cell)) return "not enough AP";  // reachable on its movement alone: only the pool is short
  return apMoveBudget(u, movePoints(u), false) <= 0 ? "that unit has no movement left" : "the unit cannot reach that cell";
}

void Battle::apPayAttack(const Action& action, std::vector<Event>& events) {
  if (!db->rules.ap.on) return;
  Unit& u = units[(size_t)action.unit];
  changeResource(u.side, db->rules.ap.resource, -apAttackCost(action.unit, action.weapon), events);
  ++u.apAttacks;
}

void Battle::apPayMove(int unit, int points, std::vector<Event>& events) {
  if (!db->rules.ap.on) return;
  const Unit& u = units[(size_t)unit];
  changeResource(u.side, db->rules.ap.resource, -apMoveCost(u, points), events);
}

void Battle::apAfterAttack(Unit& u) {
  // Not `done` while it has an attack or some movement left: the player picks it again, the computer opponent goes on with it. What the pool
  // can no longer pay for is not decided here (unitSpent asks the pool each time).
  if (apAttacksLeft(u) <= 0 && apMoveBudget(u, movePoints(u), false) <= 0) u.done = true;
}

}  // namespace ote
