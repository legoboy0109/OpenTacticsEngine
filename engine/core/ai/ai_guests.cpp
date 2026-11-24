// Guests (ai.h aiGuestStep; docs/SRPG.md "Guests", docs/AI.md "Guests"): the computer moves the guests of a side a person plays, with the same
// planner as its own units, one decision per call. Nothing here runs in a game without guests (guestToAct() is -1).
#include "core/ai/ai.h"
#include "core/ai/ai_internal.h"

namespace ote {

bool aiGuestStep(Battle& battle, std::vector<Event>& events) {
  if (battle.guestToAct() < 0) return false;
  if (battle.db->rules.speed.on) {  // its own turn: the unit part of the computer's speed step (no power, research or upgrade of the person's side)
    aiSpeedUnitStep(battle, events);
    return battle.guestToAct() >= 0;
  }
  const int side = battle.activeSide;
  ai::updateWakes(battle);  // a guest that waits (a guard behaviour) wakes up as a computer unit would
  ai::Context cx(battle);
  ai::assignCaptureGoals(cx);
  int bestUnit = -1;
  ai::Order best;
  for (const Unit& u : battle.units) {
    if (!u.guest || !u.alive || u.cell == kNoCell || u.side != side || u.done) continue;
    ai::Order o = ai::bestOrder(cx, u);
    if (bestUnit < 0 || o.score > best.score) {  // strict: ties go to the lowest unit id, as for the computer's own units
      bestUnit = u.id;
      best = o;
    }
  }
  if (bestUnit < 0) return false;
  ai::playOrder(battle, bestUnit, best, events, false);  // never EndTurn: the turn is the person's
  return battle.guestToAct() >= 0;
}

}  // namespace ote
