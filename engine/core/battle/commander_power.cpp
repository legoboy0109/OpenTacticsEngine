// Using a commander power (docs/COMMANDERS.md "Powers"): the UsePower action pays the meter, switches the timed side-wide skills on and applies the
// instant effects. The effects reuse what exists: healing and hurting are the UnitHealed / UnitHurt entries of a skill block's effects, resources the
// story command's giveResource, "refresh" the Refresh ability's.
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

namespace {

Event unitEvent(EventType t, int unit, int amount, int c = -1) {
  Event e;
  e.type = t;
  e.a = unit;
  e.b = amount;
  e.c = c;
  return e;
}

}  // namespace

void Battle::powerEffects(int side, const CommanderPower& p, std::vector<Event>& events) {
  for (const CommanderEffect& fx : p.effects) {
    SideModifier filter;  // the unit filter, in the form modifierApplies judges
    filter.filterType = fx.filterType;
    filter.filterTag = fx.filterTag;
    switch (fx.kind) {
      case CommanderEffect::Kind::Heal:
        for (Unit& u : units) {
          if (!u.alive || u.cell == kNoCell || u.side != side || !modifierApplies(filter, u)) continue;
          const int gain = std::min(fx.amount, maxHp(u) - u.hp);
          if (gain <= 0) continue;
          u.hp += gain;
          events.push_back(unitEvent(EventType::UnitHealed, u.id, gain));
        }
        break;
      case CommanderEffect::Kind::Damage:  // every hostile unit on the map, seen or not; it never defeats (leaves 1 hit point)
        for (Unit& u : units) {
          if (!u.alive || u.cell == kNoCell || teamOf(u) == sides[(size_t)side].team || !modifierApplies(filter, u)) continue;
          const int loss = std::min(fx.amount, u.hp - 1);
          if (loss <= 0) continue;
          u.hp -= loss;
          events.push_back(unitEvent(EventType::UnitHurt, u.id, loss, -1));
          wakeUnit(u.id);  // a guard that is hurt wakes up, as when it is struck
        }
        break;
      case CommanderEffect::Kind::GiveResource:
        for (const Amount& a : fx.resources) {
          // As the story's giveResource: scaled for a human side by the difficulty, never past the save limit.
          const long long now = sides[(size_t)side].resources[(size_t)a.resource];
          const long long to = std::min((long long)kMaxStoryNumber, now + resourceGain(side, a.amount));
          changeResource(side, a.resource, (int)(to - now), events);
        }
        break;
      case CommanderEffect::Kind::Refresh:
        for (Unit& u : units) {
          if (!u.alive || u.cell == kNoCell || u.side != side || !modifierApplies(filter, u)) continue;
          if (speedGame()) {  // as the Refresh ability: it acts as soon as the order reaches it (not the unit that has the turn, nor one that is ready)
            if (u.id == activeUnit || u.ct >= db->rules.speed.threshold) continue;
            u.ct = db->rules.speed.threshold;
          } else {
            if (!u.moved && !u.done && !u.acted) continue;  // nothing to give back
            u.moved = u.done = u.acted = false;
            u.moveSpent = 0;
            u.apAttacks = 0;
          }
          Event e;
          e.type = EventType::UnitRefreshed;
          e.a = u.id;  // (no unit does it: the refreshed unit is named twice)
          e.b = u.id;
          events.push_back(e);
        }
        break;
    }
  }
}

void Battle::applyUsePower(const Action& action, std::vector<Event>& events) {
  const int side = activeSide;
  SideState& s = sides[(size_t)side];
  const CommanderPower& p = commanderOf(side)->powers[(size_t)action.power];
  const int cost = powerCost(side, action.power);
  s.meter -= cost;
  ++s.powerUses[(size_t)action.power];
  Event e;
  e.type = EventType::PowerUsed;
  e.a = side;
  e.b = s.commander;
  e.c = action.power;
  e.d = cost;
  e.data.push_back(s.meter);
  events.push_back(e);
  // A power with timed skills becomes the side's active power (the meter rests until it ends); one with instant effects only is over at once.
  if (!p.modifiers.empty()) {
    s.power = action.power;
    s.powerLeft = p.turns;
    std::vector<SideModifier> mods;
    commanderPowerModifiers(*db, s.commander, action.power, mods);
    for (const SideModifier& m : mods) addSideModifier(side, m);
    for (Unit& u : units)  // a skill that raised the maximum: nothing to clamp now, but a lowering one must
      if (exists(u) && u.side == side) clampHp(u);
  }
  powerEffects(side, p, events);
}

}  // namespace ote
