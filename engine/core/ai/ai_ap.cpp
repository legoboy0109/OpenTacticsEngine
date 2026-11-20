// The computer opponent in a game with action points (docs/AI.md "Action points"). One decision per call, like aiTakeStep, but the unit does not
// get "one order": the side has one pool, so every call spends it where it buys most.
//
//   1. Every ready unit's best ATTACK (from where it stands or after a walk) and CAPTURE are scored as the usual AI scores them; the one with the
//      highest score per AP spent (walk + attack) is made. Ties go to the lowest unit id, then the first square in cell order.
//   2. When nothing like that is affordable anywhere, the unit that gets nearest to its goal for its AP advances (the same goals as the usual AI:
//      its capture target, else the nearest enemy), a square at a time, never to a square that is not nearer.
//   3. With nothing left to do, the side builds / researches / upgrades as the usual AI does, then ends its turn.
//
// Why it ends: every call either spends something finite (an attack counts against attacksPerTurn, a walk against the unit's Move stat, a capture
// ends the unit's turn) or ends the turn; a refused action sends the unit to Wait. It uses no randomness of its own. Not used here: the behaviours,
// items, staves, abilities and places of the usual AI (a game that needs them plays them as a human would); the guide is docs/AI.md.
#include <algorithm>

#include "core/ai/ai.h"
#include "core/ai/ai_internal.h"

namespace ote {

namespace {

struct Play {
  bool any = false;
  double value = 0;  // score per AP
  int unit = -1;
  Cell moveTo = kNoCell;
  ActionType kind = ActionType::Wait;  // Attack, Capture or Wait (= only walk)
  int target = -1, weapon = -1;
  Cell cell = kNoCell;
};

bool beats(const Play& p, double value) { return !p.any || value > p.value; }  // strict: the first one found keeps a tie

// A unit that may take orders now (not done, not asleep or acting on its own).
bool ready(const Battle& b, const Unit& u) { return u.alive && u.side == b.activeSide && !b.unitSpent(u) && b.validate(Action::wait(u.id)); }

void considerStrikes(ai::Context& cx, const Unit& u, Play& best) {
  const Battle& b = cx.b;
  const int pool = b.apPool(u.side);
  Reach reach;
  b.reachable(u.id, reach);  // already cut to what the pool pays for
  std::vector<AttackOption> options;
  for (Cell s : reach.stops) {
    const int travel = s == u.cell ? 0 : b.apMoveCost(u, reach.cost[(size_t)s]);
    if (ai::knownCapturable(b, u, s) && cx.mayCapture(u, s)) {
      const double sc = ai::scoreCapture(b, u, s);
      if (sc > 0 && beats(best, sc / std::max(1, travel))) {
        best = Play();
        best.any = true;
        best.value = sc / std::max(1, travel);
        best.unit = u.id;
        best.moveTo = s == u.cell ? kNoCell : s;
        best.kind = ActionType::Capture;
      }
    }
    if (b.apAttacksLeft(u) <= 0) continue;
    b.attackOptions(u.id, s, options);
    for (const AttackOption& o : options) {
      const int total = travel + b.apAttackCost(u.id, o.weapon);
      if (total > pool || (o.mapWeapon && s != u.cell)) continue;  // (it does not walk to a map weapon)
      double sc = o.area ? ai::scoreAreaAttack(b, u, o.weapon, o.cell, s) : ai::scoreAttack(b, u, b.units[(size_t)o.target], o.weapon, s);
      if (sc <= 0) continue;
      sc -= ai::exposureCost(b, u, s);  // standing where the enemy can gang up on it counts against the attack
      if (sc <= 0) continue;
      const double value = sc / std::max(1, total);
      if (!beats(best, value)) continue;
      best = Play();
      best.any = true;
      best.value = value;
      best.unit = u.id;
      best.moveTo = s == u.cell ? kNoCell : s;
      best.kind = ActionType::Attack;
      best.target = o.target;
      best.weapon = o.weapon;
      best.cell = o.area ? o.cell : kNoCell;
    }
  }
}

// A leader whose death loses the game stays at home while it has company (the usual AI's rule).
bool leaderStaysHome(const Battle& b, const Unit& u) {
  if (!u.leader || !b.db->rules.defeatLeaderDead) return false;
  for (const Unit& o : b.units)
    if (o.alive && o.side == u.side && o.id != u.id && !o.leader) return true;
  return false;
}

void considerAdvance(ai::Context& cx, const Unit& u, Play& best) {
  const Battle& b = cx.b;
  if (leaderStaysHome(b, u) || b.apMovePointsLeft(u) <= 0) return;
  const std::vector<int>& field = b.typeOf(u).canCapture && (size_t)u.id < cx.assigned.size() && cx.assigned[(size_t)u.id] != kNoCell
                                      ? cx.goalFieldFor(u, cx.assigned[(size_t)u.id])
                                      : cx.enemyFieldFor(u);  // (height rules: around the cliffs the unit cannot climb)
  const int here = field[(size_t)u.cell];
  Reach reach;
  b.reachable(u.id, reach);
  Cell pick = kNoCell;
  int pickDist = here, pickCost = 0;
  for (Cell s : reach.stops) {
    const int d = field[(size_t)s];
    if (s == u.cell || d >= ai::kFar || d >= here) continue;  // only nearer than where it stands
    const int cost = b.apMoveCost(u, reach.cost[(size_t)s]);
    if (d < pickDist || (d == pickDist && cost < pickCost)) {
      pick = s;
      pickDist = d;
      pickCost = cost;
    }
  }
  if (pick == kNoCell) return;
  const double value = (here - pickDist) * 100.0 / std::max(1, pickCost);  // distance gained per AP
  if (!beats(best, value)) return;
  best = Play();
  best.any = true;
  best.value = value;
  best.unit = u.id;
  best.moveTo = pick;
}

}  // namespace

bool aiTakeApStep(Battle& battle, std::vector<Event>& events) {
  const int side = battle.activeSide;
  const int round = battle.round;
  ai::Context cx(battle);
  ai::assignCaptureGoals(cx);

  Play play;
  for (const Unit& u : battle.units)
    if (ready(battle, u)) considerStrikes(cx, u, play);
  if (!play.any)  // nothing worth hitting or capturing anywhere: the walk toward the goal
    for (const Unit& u : battle.units)
      if (ready(battle, u)) considerAdvance(cx, u, play);

  if (play.any) {
    bool ok = true;
    if (play.moveTo != kNoCell) {
      ok = battle.apply(Action::move(play.unit, play.moveTo), events);
      if (ok && battle.units[(size_t)play.unit].cell != play.moveTo) ok = false;  // stopped short (fog ambush): not the square the plan was made for
    }
    if (ok && play.kind == ActionType::Attack) {
      Action shot = Action::attack(play.unit, play.target, play.weapon);
      shot.cell = play.cell;
      ok = battle.apply(shot, events);
    } else if (ok && play.kind == ActionType::Capture) {
      ok = battle.apply(Action::capture(play.unit), events);
    }
    // A refused order (an ambush stopped the walk, say) must not repeat forever: the unit stands down.
    if (!ok && battle.units[(size_t)play.unit].alive && !battle.units[(size_t)play.unit].done && !battle.apply(Action::wait(play.unit), events))
      battle.apply(Action::endTurn(), events);
    return !battle.over && battle.activeSide == side && battle.round == round;
  }

  int tech;
  if (ai::chooseResearch(battle, tech) && battle.apply(Action::research(tech), events))
    return !battle.over && battle.activeSide == side && battle.round == round;
  Cell cell;
  int type;
  if (ai::chooseBuild(battle, cell, type) && battle.apply(Action::build(cell, type), events))
    return !battle.over && battle.activeSide == side && battle.round == round;
  int structure, to;
  if (ai::chooseUpgrade(battle, structure, to) && battle.apply(Action::upgrade(structure, to), events))
    return !battle.over && battle.activeSide == side && battle.round == round;

  battle.apply(Action::endTurn(), events);
  return false;
}

}  // namespace ote
