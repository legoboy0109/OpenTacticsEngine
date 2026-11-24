// The computer opponent in a game with the speed turn order (docs/AI.md "Speed turn order"). One decision per call, like aiTakeStep, but only for
// the unit that has the turn now:
//
//   1. A unit that is charging an action only waits (it may not act; moving it would not help the action).
//   2. The unit's best order is found by the usual scoring (ai::bestOrder): the walk, the attack, the capture, the staff, ...
//   3. A CHARGED weapon (weapons.json "chargeSpeed") is then scored the same way and replaces that order when it scores higher. Where the action
//      would go off in the turn order is asked of Battle::turnPreview, and that decides how far the score can be trusted:
//        * it is not counted on at all when it would go off beyond twice the preview length (too slow to matter);
//        * aimed at a square (an area weapon's default): only when every enemy it would hit has its turn AFTER the action goes off (such a unit
//          cannot walk out of the blast), unless the action follows the unit;
//        * following a unit: the score is cut to 90%, and to 70% when that unit has a turn before the action goes off (it may step out of range).
//      Charged ABILITIES are not used by the computer (ai_abilities.cpp skips them).
//   4. The side's research and upgrades are looked at first, at the start of a unit's turn (a speed game has no building).
//
// Why it ends: every call either plays the unit's order and ends its turn (an attack, a charged declaration or a Wait ends it; a refused order
// sends the unit to Wait) or spends something finite (a research or an upgrade). Nothing here uses randomness.
#include <algorithm>

#include "core/ai/ai.h"
#include "core/ai/ai_internal.h"

namespace ote {

namespace {

constexpr double kAttackBase = 200;      // as an attack's (ai_order.cpp): any worthwhile action outranks plain advancing
constexpr double kMoveCostWeight = 0.5;  // prefer the shorter trip between equal actions

// The best charged weapon order of the unit that beats `beat`; false when none does.
bool chargedOrder(ai::Context& cx, const Unit& u, double beat, ai::Order& out) {
  const Battle& b = cx.b;
  std::vector<int> weapons;
  b.weaponsOf(u, weapons);
  bool any = false;
  for (int w : weapons) any = any || b.weaponCharges(w);
  std::vector<int> have, strikes;  // charged abilities that strike (docs/AI.md "Abilities that strike"): judged like charged weapons
  b.abilitiesOf(u, have);
  for (int a : have)
    if (b.abilityCharges(a) && b.abilityStrikeWeapon(a) >= 0) any = true;
  if (!any) return false;

  Reach reach;
  b.reachable(u.id, reach);
  std::vector<Cell> stops;
  if (u.moved && !u.acted) stops.push_back(u.cell);  // already moved: it acts from where it stands
  else stops = reach.stops;
  const int count = b.db->rules.speed.preview * 2;
  std::vector<AttackOption> options;
  std::vector<TurnEntry> order;
  double bestScore = beat;
  bool found = false;
  for (Cell s : stops) {
    const int travel = s == u.cell ? 0 : reach.cost[(size_t)s];
    b.attackOptions(u.id, s, options);
    std::vector<int> owner(options.size(), -1);  // per option: the strike ability it belongs to, -1 = a weapon of the unit's own
    ai::strikeAbilitiesAt(b, u, s, true, strikes);
    for (int a : strikes) {
      std::vector<AttackOption> more;
      ai::strikeOptionsFor(b, u, a, s, more);
      options.insert(options.end(), more.begin(), more.end());
      owner.resize(options.size(), a);
    }
    for (size_t oi = 0; oi < options.size(); ++oi) {
      const AttackOption& o = options[oi];
      const int ability = owner[oi];
      if (ability < 0 && (!b.weaponCharges(o.weapon) || o.mapWeapon)) continue;
      const PendingAction p = ability < 0 ? b.chargeFor(u.id, true, o.weapon, o.target, o.area ? o.cell : kNoCell, -1)
                                          : b.chargeFor(u.id, false, ability, o.area ? -1 : o.target, o.area ? o.cell : kNoCell, -1);
      b.turnPreview(count, order, &p);
      int landing = -1;
      for (size_t i = 0; i < order.size(); ++i)
        if (order[i].forecast) landing = (int)i;
      if (landing < 0) continue;  // goes off too late to count on
      auto actsBefore = [&](int unit) {
        for (int i = 1; i < landing; ++i)  // (entry 0 is the unit itself)
          if (!order[(size_t)i].charged && order[(size_t)i].unit == unit) return true;
        return false;
      };
      double sc;
      Battle::StrikeScope scope(b, u.id, ability);  // a strike ability's forecasts are those of its weapon (ability -1: none)
      if (o.area) {
        AreaForecast f;
        if (!b.areaForecast(u.id, o.weapon, s, o.cell, f) || f.victims.empty()) continue;
        if (!p.follow) {  // aimed at a square: an enemy that has its turn before it lands could walk out of the blast
          bool safe = true;
          for (const AreaVictim& v : f.victims) safe = safe && !(v.enemy && actsBefore(v.unit));
          if (!safe) continue;
        }
        sc = ai::scoreAreaAttack(b, u, o.weapon, o.cell, s);
      } else {
        sc = ai::scoreAttack(b, u, b.units[(size_t)o.target], o.weapon, s);
      }
      if (ability >= 0 && !b.db->abilities[(size_t)ability].spend.empty()) sc *= 0.9;  // (as scoreStrike: a priced strike gives way to a free one)
      if (sc <= 0) continue;
      if (p.follow) sc *= actsBefore(p.target) ? 0.7 : 0.9;
      sc -= ai::exposureCost(b, u, s);
      if (sc <= 0) continue;
      sc += kAttackBase - kMoveCostWeight * travel;
      if (sc <= bestScore) continue;  // strict: the first found keeps a tie, and an instant action wins an equal score
      bestScore = sc;
      found = true;
      out = ai::Order();
      out.score = sc;
      out.moveTo = s == u.cell ? kNoCell : s;
      if (ability >= 0) {  // used as an ability: declaring it starts the charge
        out.kind = ActionType::Ability;
        out.ability = ability;
        out.target = o.area ? -1 : o.target;
        out.cell = o.area ? o.cell : kNoCell;
        continue;
      }
      out.kind = ActionType::Attack;
      out.target = o.target;
      out.weapon = o.weapon;
      out.cell = o.area ? o.cell : kNoCell;
    }
  }
  return found;
}

}  // namespace

bool aiTakeSpeedStep(Battle& battle, std::vector<Event>& events) {
  if (battle.over || battle.activeUnit < 0) return false;
  const int uid = battle.activeUnit, turn = battle.turnNo;
  auto mine = [&]() { return !battle.over && battle.turnNo == turn && battle.activeUnit == uid; };

  if (ai::usePowerIfWorthIt(battle, events)) return mine();  // docs/COMMANDERS.md: a power may be used while the side's own unit has the turn
  int tech;
  if (ai::chooseResearch(battle, tech) && battle.apply(Action::research(tech), events)) return mine();
  int structure, to;
  if (ai::chooseUpgrade(battle, structure, to) && battle.apply(Action::upgrade(structure, to), events)) return mine();

  const Unit& u = battle.units[(size_t)uid];
  if (battle.charging(uid)) {  // its action is on its way: it only waits
    if (!battle.apply(Action::wait(uid, ai::endFacing(battle, u)), events)) battle.apply(Action::endTurn(), events);
    return mine();
  }
  ai::updateWakes(battle);  // docs/AI.md: guards that have a reason wake up before anyone chooses
  ai::updateLooters(battle);  // docs/AI.md "Looting and escaping"
  ai::Context cx(battle);
  ai::assignCaptureGoals(cx);
  ai::Order best = ai::bestOrder(cx, u);
  ai::Order charged;
  if (!u.acted && chargedOrder(cx, u, best.score, charged)) best = charged;
  ai::playOrder(battle, uid, best, events);
  return mine();
}

}  // namespace ote
