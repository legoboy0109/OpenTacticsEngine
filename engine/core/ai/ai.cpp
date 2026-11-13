// Orchestration: one decision per call (see ai.h).
#include "core/ai/ai.h"

#include "core/ai/ai_internal.h"

namespace ote {

namespace {

// True while the same side still holds the turn (nothing ended it, no
// counterattack wiped the side out, battle not over).
bool stillMyTurn(const Battle& b, int side, int round) {
  return !b.over && b.activeSide == side && b.round == round;
}

}  // namespace

bool aiTakeStep(Battle& battle, std::vector<Event>& events) {
  if (battle.over) return false;
  const int side = battle.activeSide;
  const int round = battle.round;

  // 1. Best order over all ready units; the single best one goes first so
  //    strong attacks happen early and free up cells for the rest.
  ai::Context cx(battle);
  ai::assignCaptureGoals(cx);
  int bestUnit = -1;
  ai::Order best;
  for (const Unit& u : battle.units) {
    if (!u.alive || u.side != side || u.done) continue;
    ai::Order o = ai::bestOrder(cx, u);
    if (bestUnit < 0 || o.score > best.score) {  // strict: ties go to the lowest unit id
      bestUnit = u.id;
      best = o;
    }
  }

  if (bestUnit >= 0) {
    if (best.moveTo != kNoCell) battle.apply(Action::move(bestUnit, best.moveTo), events);
    bool ok = false;
    if (best.kind == ActionType::Attack)
      ok = battle.apply(Action::attack(bestUnit, best.target, best.weapon), events);
    else if (best.kind == ActionType::Capture)
      ok = battle.apply(Action::capture(bestUnit), events);
    // Whatever happened, never leave the unit ready: that is what
    // guarantees every turn terminates.
    if (!ok && battle.units[bestUnit].alive && !battle.units[bestUnit].done &&
        !battle.apply(Action::wait(bestUnit), events))
      battle.apply(Action::endTurn(), events);
    return stillMyTurn(battle, side, round);
  }

  // 2. Everyone has acted: spend money (units standing on factories have
  //    left or finished, so their cells are free or deliberately blocked).
  Cell cell;
  int type;
  if (ai::chooseBuild(battle, cell, type) && battle.apply(Action::build(cell, type), events))
    return stillMyTurn(battle, side, round);

  // 3. Nothing left to do.
  battle.apply(Action::endTurn(), events);
  return false;
}

}  // namespace ote
