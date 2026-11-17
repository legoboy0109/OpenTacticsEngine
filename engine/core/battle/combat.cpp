// Combat: targeting, forecasts and resolving an Attack action. The numbers (modifiers, one side's strike numbers,
// the order of strikes) live in combat_mods.cpp; this file runs the fight (docs/SRPG.md "Combat pipeline").
#include <algorithm>
#include <cmath>

#include "core/battle/battle.h"
#include "core/battle/combat_mods.h"
#include "core/script/formulas.h"

namespace ote {

// Looks up the weapon's effectiveness against a target. Returns false when
// the weapon cannot target that unit at all.
bool Battle::versusValue(int weapon, const Unit& target, double& vs) const {
  const WeaponDef& w = db->weapons[weapon];
  vs = 1;
  if (w.versus.empty()) return true;
  const UnitTypeDef& type = typeOf(target);
  if (propFind(w.versus, type.id, vs)) return true;
  for (const std::string& tag : type.tags)
    if (propFind(w.versus, tag, vs)) return true;
  if (w.hasVersusDefault) {
    vs = w.versusDefault;
    return true;
  }
  return false;
}

bool Battle::weaponInRange(int weapon, int dist) const {
  const WeaponDef& w = db->weapons[weapon];
  return dist >= w.minRange && dist <= w.maxRange;
}

void Battle::attackOptions(int unit, Cell from, std::vector<AttackOption>& out) const {
  out.clear();
  if (unit < 0 || unit >= (int)units.size() || !units[unit].alive || !grid.valid(from)) return;
  const Unit& u = units[unit];
  bool moved = u.moved || from != u.cell;
  std::vector<int> weapons;
  weaponsOf(u, weapons);
  for (int w : weapons) {
    if (moved && !db->weapons[w].afterMove) continue;
    for (const Unit& t : units) {
      if (!t.alive || !isEnemy(u, t) || !unitVisibleTo(teamOf(u), t)) continue;  // fog: only what the team sees
      double vs;
      if (!weaponInRange(w, grid.distance(from, t.cell)) || !versusValue(w, t, vs)) continue;
      AttackOption opt;
      opt.target = t.id;
      opt.weapon = w;
      out.push_back(opt);
    }
  }
}

bool Battle::forecast(int attacker, int defender, int weapon, Cell from, Forecast& out) const {
  CombatSetup setup;
  return forecastFight(attacker, defender, weapon, from, out, setup);
}

bool Battle::forecastFight(int attacker, int defender, int weapon, Cell from, Forecast& out,
                           CombatSetup& setup) const {
  out = Forecast();
  if (attacker < 0 || attacker >= (int)units.size() || defender < 0 ||
      defender >= (int)units.size() || !grid.valid(from))
    return false;
  const Unit& a = units[attacker];
  const Unit& d = units[defender];
  if (!a.alive || !d.alive || !mayFight(a, d) || !unitVisibleTo(teamOf(a), d)) return false;  // fog: not what it cannot see
  std::vector<int> owned;
  weaponsOf(a, owned);
  if (std::find(owned.begin(), owned.end(), weapon) == owned.end()) return false;
  if ((a.moved || from != a.cell) && !db->weapons[weapon].afterMove) return false;
  if (!weaponInRange(weapon, grid.distance(from, d.cell))) return false;

  CombatMods modsA, modsD;
  setUpCombat(attacker, defender, weapon, from, setup, modsA, modsD);
  if (!strikeForecast(setup, false, modsA, modsD, out.attack)) return false;
  if (setup.counters && !strikeForecast(setup, true, modsD, modsA, out.counter)) setup.counters = false;
  out.defenderFirst = setup.counters && modsD.strikeFirst && !modsA.strikeFirst;

  // "strike" / "struck" blocks without a chance are certain, so they are part of the numbers; activations are
  // only listed (docs/SRPG.md "Forecast").
  for (int side = 0; side < 2; ++side) {
    bool counter = side == 1;
    StrikeForecast& numbers = counter ? out.counter : out.attack;
    if (!numbers.can) continue;
    StrikeMods strike;
    strikePassives(setup, counter, strike);
    if (testMods && testMods->passive) testMods->passive(*this, setup, counter, strike);
    foldStrikeMods(strike, numbers);
    noDodge(setup, counter, numbers);
    numbers.damage = applyPercents(applyPercents(numbers.damage, strike.damagePercent), strike.damageTakenPercent);
  }
  forecastNotes(setup, out.notes);
  if (testMods && testMods->notes) testMods->notes(*this, setup, out.notes);
  return true;
}

// One strike: steps 1 to 8 of docs/SRPG.md "Combat pipeline". With no source of modifiers this makes exactly the
// random rolls and events it made before the pipeline: at most one for the hit and one for the critical.
void Battle::doStrike(const CombatSetup& setup, bool counter, bool extra, std::vector<Event>& events) {
  int atk = counter ? setup.defender : setup.attacker;
  int def = counter ? setup.attacker : setup.defender;
  int weapon = counter ? setup.weaponD : setup.weaponA;
  // 1. The numbers. Recomputed per strike on purpose: earlier strikes change hp, many formulas (Advance Wars
  // style) scale damage with current hp, and "combat" skill blocks may depend on it too.
  CombatMods modsA, modsD;
  combatMods(setup, modsA, modsD);
  StrikeForecast f;
  if (!strikeForecast(setup, counter, counter ? modsD : modsA, counter ? modsA : modsD, f)) return;
  // A carried weapon: gone (broken earlier in this fight) means no strike.
  int slot = -1;
  if (db->rules.items.carriedWeapons && (slot = weaponSlot(units[atk], weapon)) < 0) return;

  // 2. This strike's own effects: the striker's "strike" blocks, then the struck unit's "struck" blocks.
  // Activations among them are rolled here, before the hit roll.
  StrikeMods strike;
  std::vector<Event> strikeEvents, struckEvents;
  strikeActivations(setup, counter, strike, strikeEvents, struckEvents);
  if (testMods && testMods->strike) testMods->strike(*this, setup, counter, strike, strikeEvents, struckEvents);
  foldStrikeMods(strike, f);
  noDodge(setup, counter, f);

  Event e;
  e.type = EventType::Attack;
  e.a = atk;
  e.b = def;
  e.c = weapon;
  e.d = 0;
  e.flags = counter ? kCounter : 0;
  // 3. Hit. (A hit of 0 still rolls, as it always did.)
  bool hit = f.hit >= 100 || rng.chance(f.hit);
  if (hit) {
    // 4. Damage: a critical multiplies first, then this strike's percents, then "survive".
    int damage = f.damage;
    if (f.crit > 0 && rng.chance(f.crit)) {
      damage = (int)std::floor(damage * db->rules.critMultiplier);
      e.flags |= kCrit;
    }
    damage = applyPercents(applyPercents(damage, strike.damagePercent), strike.damageTakenPercent);
    Unit& target = units[def];
    if (strike.survive && target.hp > 1 && damage >= target.hp) {
      damage = target.hp - 1;
      e.flags |= kSurvived;
    }
    damage = std::min(damage, target.hp);
    target.hp -= damage;
    e.d = damage;
    e.flags |= kHit;
    if (target.hp <= 0) e.flags |= kKill;
  }
  // 5. The events: what activated for the striker, for the struck unit (only when it was struck), the strike.
  events.insert(events.end(), strikeEvents.begin(), strikeEvents.end());
  if (hit) events.insert(events.end(), struckEvents.begin(), struckEvents.end());
  events.push_back(e);
  // 6. Drain: the striker heals a share of the damage it dealt (at least 1), up to its maximum.
  if (strike.drain > 0 && e.d > 0) {
    Unit& striker = units[atk];
    int heal = std::min(std::max(1, (int)((long long)e.d * strike.drain / 100)), maxHp(striker) - striker.hp);
    if (heal > 0) {
      striker.hp += heal;
      Event h;
      h.type = EventType::UnitHealed;
      h.a = atk;
      h.b = heal;
      events.push_back(h);
    }
  }
  // 6b. Conditions (docs/SRPG.md section 9), only on a hit: a unit struck loses the conditions that end when it is struck, then
  // the "inflict" effects of the strike's activated blocks take hold (in that order, so a strike that wakes a sleeper can also
  // put it back to sleep): the striker's on the unit struck, the struck unit's on the striker.
  if (hit && db->conditions.size()) {
    if (e.d > 0) conditionsStruck(def, events);
    for (const SkillEffect* fx : strike.inflictOnStruck)
      if (units[def].alive) inflictCondition(def, fx->conds[0], fx->turns, atk, events);
    for (const SkillEffect* fx : strike.inflictOnStriker)
      if (units[atk].alive) inflictCondition(atk, fx->conds[0], fx->turns, def, events);
  }
  // 7. Wear, drops, defeat.
  if (slot >= 0) spendUse(atk, slot, events);  // every strike made wears the weapon, hit or miss
  if (e.flags & kKill) {
    // What the fallen unit was marked to drop goes to the one who struck it down.
    std::vector<ItemStack> drops;
    for (const ItemStack& s : units[def].items)
      if (s.drop) drops.push_back(s);
    killUnit(def, events, atk);
    for (const ItemStack& s : drops) giveTo(atk, s, kDropped, events);
    unitDefeated(def, atk, events);  // may run story events: nothing below may hold a reference into `units`
  }
  // 8. "Strike again": one more strike through the same steps, hit or miss; that one cannot give another.
  if (strike.strikeAgain && !extra && !over && units[atk].alive && units[def].alive)
    doStrike(setup, counter, true, events);
}

void Battle::applyAttack(const Action& action, std::vector<Event>& events) {
  int a = action.unit, d = action.target;
  Forecast f;
  CombatSetup setup;
  forecastFight(a, d, action.weapon, units[a].cell, f, setup);  // validated by caller

  // "Just before a fight": story events may change things, even take a fighter off the map or end
  // the battle, in which case there is no fight.
  StoryOccurrence fight;
  fight.kind = Trigger::Kind::Fight;
  fight.unit = a;
  fight.other = d;
  fireStory(fight, events);
  if (over || !units[a].alive || !units[d].alive ||
      !forecastFight(a, d, action.weapon, units[a].cell, f, setup)) {
    if (units[a].alive) endOrder(a);
    return;
  }

  // Attacking with a carried weapon takes it in hand.
  if (db->rules.items.carriedWeapons) {
    int slot = weaponSlot(units[a], action.weapon);
    if (slot > 0 && slot != equippedSlot(units[a])) {
      ItemStack s = units[a].items[(size_t)slot];
      units[a].items.erase(units[a].items.begin() + slot);
      units[a].items.insert(units[a].items.begin(), s);
    }
  }
  int hpA = units[a].hp, hpD = units[d].hp;
  // Who strikes when, how often, and whether the defender answers are fixed now; only the numbers of each strike
  // are worked out again when it is made.
  std::vector<StrikeStep> order;
  strikeOrder(f, order);
  bool ammoA = false, ammoD = false;
  const size_t combatFrom = events.size();  // weapon ranks read this combat's strikes off the events
  for (const StrikeStep& step : order) {
    for (int i = 0; i < step.count; ++i) {
      if (!units[a].alive || !units[d].alive) break;
      // Ammunition: one per fight, spent when its first strike is made (a counter that never comes costs nothing).
      bool& spent = step.counter ? ammoD : ammoA;
      if (!spent) spendAmmo(step.counter ? d : a, step.counter ? setup.weaponD : setup.weaponA);
      spent = true;
      doStrike(setup, step.counter, false, events);
    }
  }

  if (db->rules.progression) {
    FormulaCtx ctx;
    ctx.sides = &sides;
    ctx.battle = this;
    // Attacker's reward.
    if (units[a].alive && units[d].hp < hpD) {
      ctx.a = &units[a];
      ctx.b = &units[d];
      Formulas::Slot slot = units[d].alive ? Formulas::XpHit : Formulas::XpKill;
      awardXp(a, (int)std::floor(formulas->eval(slot, ctx, 0)), events);
    }
    // Defender's reward for a successful counter.
    if (units[d].alive && units[a].hp < hpA) {
      ctx.a = &units[d];
      ctx.b = &units[a];
      Formulas::Slot slot = units[a].alive ? Formulas::XpHit : Formulas::XpKill;
      awardXp(d, (int)std::floor(formulas->eval(slot, ctx, 0)), events);
    }
  }
  ranksAfterCombat(events, combatFrom, a, d);  // weapon ranks, growth "use" (docs/SRPG.md section 7); nothing otherwise
  // After the experience: "defeat" / "afterCombat" skills and supports' "fought together" (docs/SRPG.md "Combat
  // pipeline"), and only then the attacker's order ends (a skill may let it act again). A fallen attacker keeps
  // its flags as they were.
  srpgAfterCombat(a, d, hpA, hpD, events);
  if (units[a].alive) endOrder(a);
}

void Battle::killUnit(int unit, std::vector<Event>& events, int killer) {
  Unit& u = units[unit];
  if (!u.alive) return;
  releaseCapture(unit, kNoCell);  // a dead capturer abandons the capture
  Event e;
  e.type = EventType::UnitDied;
  e.a = unit;
  e.b = u.cell;
  events.push_back(e);
  markSight();  // pushed events are stamped while the unit is still there
  if (u.cell != kNoCell) occupant[u.cell] = -1;
  u.alive = false;
  u.hp = 0;
  u.cell = kNoCell;
  u.conditions.clear();  // a defeated unit has none (docs/SRPG.md section 9)
  if (u.leader) sides[u.side].leaderLost = true;
  // Last, and `u` is not used after it: its summons vanish, a unit it carried is put down (docs/SRPG.md).
  srpgUnitGone(unit, killer, events);
}

}  // namespace ote
