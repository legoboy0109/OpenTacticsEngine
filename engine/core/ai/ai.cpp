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
  ai::updateWakes(battle);  // docs/AI.md: guards that have a reason wake up before anyone chooses
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
    if (best.moveTo != kNoCell) {
      battle.apply(Action::move(bestUnit, best.moveTo), events);
      // Ambushed ("rules.fog.ambush": "canAct"): the unit stopped short and may still act. What it planned for the
      // square it meant to reach may not apply here, so it picks again from where it stands (the unit has `moved`,
      // so bestOrder only considers acting in place: attack if it can, else wait). With "endsTurn" it is done.
      const Unit& ub = battle.units[bestUnit];
      if (ub.alive && !ub.done && ub.cell != best.moveTo) {
        ai::Context again(battle);
        ai::assignCaptureGoals(again);
        best = ai::bestOrder(again, ub);
        best.moveTo = kNoCell;
      }
    }
    bool ok = false;
    if (best.kind == ActionType::Attack)
      ok = battle.apply(Action::attack(bestUnit, best.target, best.weapon), events);
    else if (best.kind == ActionType::Capture)
      ok = battle.apply(Action::capture(bestUnit), events);
    else if (best.kind == ActionType::UseItem)
      ok = battle.apply(Action::useItem(bestUnit, best.slot, best.target), events);
    else if (best.kind == ActionType::UseStaff)
      ok = battle.apply(Action::useStaff(bestUnit, best.slot, best.target), events);
    else if (best.kind == ActionType::Supply)
      ok = battle.apply(Action::supply(bestUnit), events);
    else if (best.kind == ActionType::Promote)
      ok = battle.apply(Action::promote(bestUnit), events);
    else if (best.kind == ActionType::Ability) {
      Action use = Action::useAbility(bestUnit, best.ability, best.target, best.cell);
      use.slot = best.slot;  // a steal: which of the foe's items
      ok = battle.apply(use, events);
    } else if (best.kind == ActionType::OpenPlace)
      ok = battle.apply(Action::openPlace(bestUnit, best.structure, best.slot), events);
    else if (best.kind == ActionType::VisitPlace)
      ok = battle.apply(Action::visitPlace(bestUnit, best.structure), events);
    // Whatever happened, never leave the unit ready: that is what
    // guarantees every turn terminates.
    if (!ok && battle.units[bestUnit].alive && !battle.units[bestUnit].done &&
        !battle.apply(Action::wait(bestUnit), events))
      battle.apply(Action::endTurn(), events);
    return stillMyTurn(battle, side, round);
  }

  // 2. Everyone has acted: spend money (units standing on factories have
  //    left or finished, so their cells are free or deliberately blocked).
  //    Research comes first so that it keeps enough back to still build; upgrades come last, from what is left.
  int tech;
  if (ai::chooseResearch(battle, tech) && battle.apply(Action::research(tech), events))
    return stillMyTurn(battle, side, round);
  Cell cell;
  int type;
  if (ai::chooseBuild(battle, cell, type) && battle.apply(Action::build(cell, type), events))
    return stillMyTurn(battle, side, round);
  int structure, to;
  if (ai::chooseUpgrade(battle, structure, to) && battle.apply(Action::upgrade(structure, to), events))
    return stillMyTurn(battle, side, round);

  // 3. Nothing left to do.
  battle.apply(Action::endTurn(), events);
  return false;
}

}  // namespace ote
