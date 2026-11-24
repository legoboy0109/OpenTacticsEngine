// Resolving an attack with an area weapon (docs/SRPG.md "Area attacks and line fire"). Every unit in the area is struck with the normal
// strike (doStrike), so damage types, terrain and skills count per victim. The rules in one place:
//   * victims are struck in unit-id order; the random stream is used in that order, and each victim makes the same rolls a single fight
//     with it would make for one strike (hit, critical, skill activations);
//   * deaths are applied after the last victim was struck (`pendingKills_`), then their defeat events run in the same order;
//   * only the unit on the aimed square may counter-attack, once, after the attacker's strikes, and only if the normal rules let it;
//   * every victim is struck once: no follow-up strikes, no "strike again" for the attacker, no strikes in a row for the attacker;
//   * the weapon wears once, the ammunition is spent once;
//   * experience: the attacker earns its usual reward for each enemy victim (the unit on the aimed square: the whole fight's, with the
//     counter-attack's reward for the defender); allies hit by friendly fire earn nothing.
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

void Battle::applyAreaAttack(const Action& action, std::vector<Event>& events) {
  const int a = action.unit, weapon = action.weapon;
  const Cell target = aimedCell(action);
  AreaForecast f;
  if (!areaForecast(a, weapon, units[a].cell, target, f) && f.victims.empty()) {  // (validated by the caller; berserk may not have been)
    if (units[a].alive && strikeWeapon_ < 0) endOrder(a, true);  // (a strike ability: applyAbility ends the order)
    return;
  }
  int primary = f.primary >= 0 ? f.victims[(size_t)f.primary].unit : -1;

  // "Just before a fight", as for any attack, with the unit on the aimed square when there is one (an empty square has nobody to talk).
  // Story events may change things, even end the battle, in which case there is no attack.
  if (primary >= 0 && isEnemy(units[a], units[primary])) {
    fightQuotes(a, primary, events);
    StoryOccurrence fight;
    fight.kind = Trigger::Kind::Fight;
    fight.unit = a;
    fight.other = primary;
    fireStory(fight, events);
    if (over || !units[a].alive || !areaForecast(a, weapon, units[a].cell, target, f)) {
      if (units[a].alive && strikeWeapon_ < 0) endOrder(a, true);
      return;
    }
    primary = f.primary >= 0 ? f.victims[(size_t)f.primary].unit : -1;
  }
  faceToward(a, target, events);  // rules.facing: the attacker turns to the square it aims at; the victims keep their facings
  const bool ability = strikeWeapon_ >= 0;  // a strike ability (ability_strike.cpp): no weapon in hand, no wear, ammunition or rank
  if (!ability) takeWeaponInHand(a, weapon);

  const int hpA = units[a].hp;
  std::vector<int> hpBefore;
  for (const AreaVictim& v : f.victims) hpBefore.push_back(units[(size_t)v.unit].hp);
  fightStrikes_[0] = fightStrikes_[1] = 0;
  fightMarks_.clear();  // marks live for one fight (skill_marks.cpp)
  const size_t combatFrom = events.size();
  const int mapStructure = mapWeaponUsedBy(units[a], units[a].cell, weapon);
  const int slot = !ability && mapStructure < 0 && db->rules.items.carriedWeapons ? weaponSlot(units[a], weapon) : -1;
  if (mapStructure < 0 && !ability) spendAmmo(a, weapon);  // one per attack, however many it hits (a map weapon has its own count)

  // The attacker's strikes, one per victim, in unit-id order. Deaths wait (doStrike pushes them to `pendingKills_`).
  fightArea_ = true;
  pendingKills_.clear();
  CombatSetup primarySetup;
  bool primaryCounters = false;
  int counterStrikes = 1;
  for (const AreaVictim& v : f.victims) {
    if (over || !units[a].alive || units[a].hp <= 0) break;
    if (!units[(size_t)v.unit].alive || units[(size_t)v.unit].hp <= 0) continue;
    Forecast one;
    CombatSetup setup;
    if (!forecastFight(a, v.unit, weapon, units[a].cell, one, setup, false, v.unit == primary && mayFight(units[a], units[(size_t)v.unit]) ? 2 : 1)) continue;
    if (v.unit == primary) {
      primarySetup = setup;
      primaryCounters = setup.counters && one.counter.can;
      counterStrikes = std::max(1, one.counter.multi);
    }
    doStrike(setup, false, true, events);  // `extra` = true: no "strike again" of the attacker's skills
  }
  // The answer of the unit on the aimed square: one counter-attack after the shot (its multi-strike skills still apply), if it is still
  // standing and the attacker too.
  if (primary >= 0 && primaryCounters && !over) {
    spendAmmo(primary, primarySetup.weaponD);  // its ammunition, once (as in a fight: when its first strike is made)
    for (int i = 0; i < counterStrikes; ++i) {
      if (units[a].hp <= 0 || !units[a].alive || units[(size_t)primary].hp <= 0 || !units[(size_t)primary].alive) break;
      doStrike(primarySetup, true, true, events);
    }
  }
  fightArea_ = false;
  // One use of the weapon, whatever it hit.
  if (slot >= 0 && !fightNoWear_ && units[a].alive) spendUse(a, slot, events);
  if (mapStructure >= 0) spendMapWeapon(mapStructure, events);

  // Now the deaths, in the order they were struck. A story event of one defeat may change what is left, so every unit is looked at again.
  std::vector<PendingKill> kills;
  kills.swap(pendingKills_);
  for (const PendingKill& k : kills) {
    if (!units[(size_t)k.unit].alive) continue;
    if (k.wielder) {
      if (!killUnit(k.unit, events)) continue;  // caught by an "autoRevive" condition
      defeatQuote(k.unit, events);
      unitDefeated(k.unit, -1, events);
    } else {
      defeatStruck(k.unit, k.killer, events);
    }
  }

  // Experience for every enemy struck (the aimed one gets the whole fight's, counter included).
  int lead = -1;  // who the "after combat" skills and supports are told about: the aimed unit, else the first enemy hit
  int leadHp = 0;
  for (size_t i = 0; i < f.victims.size(); ++i) {
    const AreaVictim& v = f.victims[i];
    if (!v.enemy || !units[a].alive) continue;
    fightExperience(a, v.unit, hpA, hpBefore[i], events, (int)i != f.primary);
    if (lead < 0 || (int)i == f.primary) {
      lead = v.unit;
      leadHp = hpBefore[i];
    }
  }
  if (!ability) ranksAfterCombat(events, combatFrom, a, primary);  // weapon ranks (growth "use"): the attacker once, the aimed unit if it answered
  // After the experience: "defeat" / "afterCombat" skills and supports' "fought together" once per attack, with the lead victim.
  if (lead >= 0 && units[a].alive) srpgAfterCombat(a, lead, hpA, leadHp, events);
  fightMarks_.clear();
  if (units[a].alive && !ability) endOrder(a, true);
}

}  // namespace ote
