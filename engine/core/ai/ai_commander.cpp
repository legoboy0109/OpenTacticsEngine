// The computer opponent and commander powers (docs/AI.md "Commanders"). Before it plays a unit, the side uses a power when it can pay for one and
// the power is worth it by these simple rules, nothing more:
//
//   * The meter is FULL (it would only be wasted): the dearest power it can pay for is used, whatever it does.
//   * Else a power is used when one of its parts would pay off now:
//       - timed skills ("modifiers"): at least two of the side's ready units (one when it has just one) have an enemy it can see within
//         "move + longest weapon range" squares (as the crow flies) - the fighting is about to start;
//       - "heal": the units it would heal are missing, all together, at least twice the amount;
//       - "damage": it would hurt at least two enemy units (more than 1 hit point left each) and the fighting has started (as above);
//       - "giveResource": always;
//       - "refresh": at least two of the units it would refresh have acted already.
//   * Of the powers that qualify, the dearest one is used (the lowest index among equals).
//
// It always ends: a used power takes the meter's points and (with timed skills) becomes active, which blocks the next one, so a step that
// uses a power changes the state and never repeats itself. No randomness: the rules read the board and the meter only. Its normal scoring
// needs nothing more: the skills a power gives are side-wide modifiers, and Battle::skillsOf (so every forecast) sees them.
#include <algorithm>

#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

namespace {

bool matches(const Battle& b, const CommanderEffect& fx, const Unit& u) {
  SideModifier f;
  f.filterType = fx.filterType;
  f.filterTag = fx.filterTag;
  return b.modifierApplies(f, u);
}

int longestRange(const Battle& b, const Unit& u) {
  std::vector<int> weapons;
  b.weaponsOf(u, weapons);
  int best = 0;
  for (int w : weapons) best = std::max(best, b.db->weapons[(size_t)w].maxRange);
  return best;
}

// Are enough of the side's ready units about to fight? (as the crow flies; enemies it cannot see do not count)
bool fightingStarts(const Battle& b, int side) {
  int ready = 0, near = 0;
  for (const Unit& u : b.units) {
    if (!u.alive || u.cell == kNoCell || u.side != side || b.unitSpent(u)) continue;
    ++ready;
    const int reach = b.movePoints(u) + longestRange(b, u);
    for (const Unit& e : b.units) {
      if (!e.alive || e.cell == kNoCell || !b.isEnemy(u, e) || !b.unitVisibleToSide(side, e.id)) continue;
      if (b.grid.distance(u.cell, e.cell) <= reach) {
        ++near;
        break;
      }
    }
  }
  return ready > 0 && near >= std::min(2, ready);
}

bool partPaysOff(const Battle& b, int side, const CommanderEffect& fx) {
  switch (fx.kind) {
    case CommanderEffect::Kind::Heal: {
      long long missing = 0;
      for (const Unit& u : b.units)
        if (u.alive && u.cell != kNoCell && u.side == side && matches(b, fx, u)) missing += std::min(fx.amount, b.maxHp(u) - u.hp);
      return missing >= 2LL * fx.amount;
    }
    case CommanderEffect::Kind::Damage: {
      int hurt = 0;
      for (const Unit& u : b.units)
        if (u.alive && u.cell != kNoCell && b.teamOf(u) != b.sides[(size_t)side].team && matches(b, fx, u) && u.hp > 1) ++hurt;
      return hurt >= 2 && fightingStarts(b, side);
    }
    case CommanderEffect::Kind::GiveResource: return true;
    case CommanderEffect::Kind::Refresh: {
      int spent = 0;
      for (const Unit& u : b.units)
        if (u.alive && u.cell != kNoCell && u.side == side && matches(b, fx, u) && (u.moved || u.done || u.acted)) ++spent;
      return spent >= 2;
    }
  }
  return false;
}

bool worthIt(const Battle& b, int side, const CommanderPower& p) {
  if (!p.modifiers.empty() && fightingStarts(b, side)) return true;
  for (const CommanderEffect& fx : p.effects)
    if (partPaysOff(b, side, fx)) return true;
  return false;
}

}  // namespace

bool usePowerIfWorthIt(Battle& b, std::vector<Event>& events) {
  const int side = b.activeSide;
  const CommanderDef* d = b.commanderOf(side);
  if (!d || b.over) return false;
  const bool full = b.meterOf(side) >= b.meterMax(side);
  int pick = -1;
  for (int i = 0; i < (int)d->powers.size(); ++i) {
    if (!b.canUsePower(side, i)) continue;
    if (!full && !worthIt(b, side, d->powers[(size_t)i])) continue;
    if (pick < 0 || b.powerCost(side, i) > b.powerCost(side, pick)) pick = i;  // strict: the lowest index among equals
  }
  return pick >= 0 && b.apply(Action::usePower(pick), events);
}

}  // namespace ai
}  // namespace ote
