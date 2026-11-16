// Carrying an ability out (docs/SRPG.md "Legality", "Cost, uses, cooldown"). The order: AbilityUsed, what the effect
// did (UnitRelocated / UnitHealed / UnitAdded), the cost, and last the story's "reach" events for units that were
// moved (target first, then the user) - they may add units or end the battle, so nothing runs after them.
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

void Battle::applyAbility(const Action& action, std::vector<Event>& events) {
  const int uid = action.unit;
  const int ability = action.ability;
  const AbilityDef& def = db->abilities[(size_t)ability];  // validated
  const int team = teamOf(units[(size_t)uid]);
  const bool onCell = def.target == AbilityTarget::Cell;
  const int target = def.target == AbilityTarget::Self ? uid : onCell ? -1 : action.target;

  Event used;
  used.type = EventType::AbilityUsed;
  used.a = uid;
  used.b = ability;
  used.c = target;
  used.d = onCell ? action.cell : -1;

  // The truth, worked out before anything changes. A needed square that an enemy the team never saw stands on makes
  // the ability blocked: it is paid for, but only what the truth allows happens.
  AbilityPlan plan;
  bool blocked = false;
  switch (def.effect) {
    case AbilityEffect::Push:
    case AbilityEffect::Pull:
    case AbilityEffect::Swap:
    case AbilityEffect::MoveBehind:
      planAbilityMove(units[(size_t)uid], def, target, plan);
      // A unit in the way that the team could see is the ordinary end of a push, not a surprise.
      blocked = plan.blocker >= 0 && !unitVisibleTo(team, plan.blocker);
      break;
    case AbilityEffect::Summon:
      plan.blocker = occupant[(size_t)action.cell];
      blocked = plan.blocker >= 0;
      break;
    case AbilityEffect::Heal: break;
  }
  if (blocked) {
    used.flags = kBlocked;
    used.data.push_back(plan.blocker);
  }
  events.push_back(used);

  switch (def.effect) {
    case AbilityEffect::Heal: {
      Unit& t = units[(size_t)target];
      const int amount = std::min(def.amount, maxHp(t) - t.hp);
      if (amount > 0) {
        t.hp += amount;
        Event h;
        h.type = EventType::UnitHealed;
        h.a = target;
        h.b = amount;
        events.push_back(h);
        if (target != uid) supportsHealed(uid, target, events);  // supports: one healed the other
      }
      break;
    }
    case AbilityEffect::Summon:
      if (!blocked) summonUnit(uid, ability, action.cell, events);
      break;
    default:
      doAbilityMoves(plan, events);
      break;
  }

  noteAbilityUse(uid, ability);
  if (!def.free) endOrder(uid);

  // A moved unit has stopped somewhere: events waiting for a unit to arrive on that square may run.
  for (const AbilityMove& m : plan.moves) {
    StoryOccurrence o;
    o.kind = Trigger::Kind::Reach;
    o.unit = m.unit;
    o.cell = m.to;
    fireStory(o, events);
  }
}

}  // namespace ote
