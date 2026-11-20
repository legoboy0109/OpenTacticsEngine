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
  const std::string* matched = &type.id;  // which key gave the number (null = the weapon's default)
  bool found = propFind(w.versus, type.id, vs);
  for (size_t i = 0; !found && i < type.tags.size(); ++i) {
    found = propFind(w.versus, type.tags[i], vs);
    if (found) matched = &type.tags[i];
  }
  if (!found && w.hasVersusDefault) {
    vs = w.versusDefault;
    matched = nullptr;
    found = true;
  }
  if (!found) return false;
  // "cancelEffective" (a skill of the target): an "effective against" bonus does not apply to it (a malus is kept). With a list of tags
  // it cancels only the effectiveness that came from one of them (a weapon's default number never matches a list).
  if (vs > 1 && db->cancelEffectiveSkills) {
    std::vector<const SkillEffect*> fx;
    standingEffects(target, false, fx);
    for (const SkillEffect* e : fx) {
      if (e->kind != SkillEffect::Kind::CancelEffective) continue;
      if (e->names.empty() || (matched && std::find(e->names.begin(), e->names.end(), *matched) != e->names.end())) {
        vs = 1;
        break;
      }
    }
  }
  return true;
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
  int mapStructure = -1;
  attackWeaponsAt(u, from, weapons, &mapStructure);  // carried or fixed weapons, plus the one of the structure it would stand on
  for (int w : weapons) {
    if (moved && !db->weapons[w].afterMove) continue;
    if (db->weapons[w].area.on()) {  // an area weapon aims at squares (area_forecast.cpp)
      areaOptions(unit, from, w, mapStructure >= 0 && w == mapWeaponOf(mapStructure), out);
      continue;
    }
    for (const Unit& t : units) {
      if (!t.alive || !isEnemy(u, t) || !unitVisibleTo(teamOf(u), t)) continue;  // fog: only what the team sees
      double vs;
      if (!weaponAims(w, from, t.cell, unit) || !versusValue(w, t, vs)) continue;  // range; line fire also needs a clear straight line
      AttackOption opt;
      opt.target = t.id;
      opt.weapon = w;
      opt.cell = t.cell;
      opt.mapWeapon = mapStructure >= 0 && w == mapWeaponOf(mapStructure);
      out.push_back(opt);
    }
  }
}

bool Battle::forecast(int attacker, int defender, int weapon, Cell from, Forecast& out) const {
  CombatSetup setup;
  return forecastFight(attacker, defender, weapon, from, out, setup);
}

bool Battle::forecastFight(int attacker, int defender, int weapon, Cell from, Forecast& out,
                           CombatSetup& setup, bool storyFight, int areaMode) const {
  out = Forecast();
  if (attacker < 0 || attacker >= (int)units.size() || defender < 0 ||
      defender >= (int)units.size() || !grid.valid(from))
    return false;
  const Unit& a = units[attacker];
  const Unit& d = units[defender];
  if (!a.alive || !d.alive) return false;
  if (!storyFight && !areaMode && (!mayFight(a, d) || !unitVisibleTo(teamOf(a), d))) return false;  // fog: not what it cannot see
  std::vector<int> owned;
  attackWeaponsAt(a, from, owned);  // (the structure it would stand on may add a weapon)
  if (std::find(owned.begin(), owned.end(), weapon) == owned.end()) return false;
  if (!storyFight && (a.moved || from != a.cell) && !db->weapons[weapon].afterMove) return false;
  if (!storyFight && !areaMode && !weaponAims(weapon, from, d.cell, attacker)) return false;  // range, and the straight-line rules of line fire

  // Everything below only reads the battle (this method is const; no roll is made), so a unit's stats cannot change between
  // the formula calls of this forecast: let the formulas work them out once (Formulas::BatchScope).
  Formulas::BatchScope formulaBatch(formulas);
  CombatMods modsA, modsD;
  setUpCombat(attacker, defender, weapon, from, setup, modsA, modsD);
  if (areaMode == 1) setup.counters = false;  // only the unit on the aimed square may answer an area attack
  if (!strikeForecast(setup, false, modsA, modsD, out.attack)) return false;
  if (setup.counters && !strikeForecast(setup, true, modsD, modsA, out.counter)) setup.counters = false;
  out.defenderFirst = setup.counters && modsD.strikeFirst && !modsA.strikeFirst;

  // "strike" / "struck" blocks without a chance are certain, so they are part of the numbers; activations are
  // only listed (docs/SRPG.md "Forecast").
  for (int side = 0; side < 2; ++side) {
    bool counter = side == 1;
    StrikeForecast& numbers = counter ? out.counter : out.attack;
    if (!numbers.can) continue;
    // The numbers of strike number `no` of this fighter (the first one for the headline numbers).
    const StrikeForecast base = numbers;
    auto strikeNumbers = [&](int no, StrikeForecast& into) {
      StrikeMods strike;
      strikePassives(setup, counter, no, strike);
      if (testMods && testMods->passive) testMods->passive(*this, setup, counter, strike);
      foldStrikeMods(strike, into);
      noDodge(setup, counter, into);
      into.damage = applyPercents(applyPercents(into.damage, strike.damagePercent), strike.damageTakenPercent);
    };
    strikeNumbers(1, numbers);
    // A game with blocks that ask which strike it is: each strike's own numbers (docs/SRPG.md "One strike of a fight"). The headline
    // numbers stay those of the first strike; `each` is only filled when some later strike differs from it.
    if (db->strikeNoUsed) {
      const int total = numbers.strikes * std::max(1, numbers.multi);
      std::vector<StrikeNumbers> list;
      bool differs = false;
      for (int no = 1; no <= total; ++no) {
        StrikeForecast one = base;
        strikeNumbers(no, one);
        list.push_back({one.damage, one.hit, one.crit});
        differs = differs || one.damage != numbers.damage || one.hit != numbers.hit || one.crit != numbers.crit;
      }
      if (differs) numbers.each = list;
    }
  }
  forecastNotes(setup, out, out.notes);
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
  // A map weapon (docs/SRPG.md "Map weapons"): spent, or the structure turned into another kind in this fight, means no strike.
  int slot = -1;
  const bool mapStrike = !counter && setup.mapStructure >= 0;
  // An area attack (area_attack.cpp) checked its weapon once and wears it once, after its last strike: its own strikes neither look at
  // the wear nor cause any (a counter-attack is an ordinary strike).
  const bool areaStrike = fightArea_ && !counter;
  if (areaStrike) {
    // (nothing to check)
  } else if (mapStrike) {
    if (mapWeaponUses(setup.mapStructure) == 0 || mapWeaponOf(setup.mapStructure) != weapon) return;
  } else if (db->rules.items.carriedWeapons && (slot = weaponSlot(units[atk], weapon)) < 0) {
    return;
  }

  // 2. This strike's own effects: the striker's "strike" blocks, then the struck unit's "struck" blocks.
  // Activations among them are rolled here, before the hit roll.
  StrikeMods strike;
  std::vector<Event> strikeEvents, struckEvents;
  const int strikeNo = ++fightStrikes_[counter ? 1 : 0];  // every strike a fighter makes counts: repeats and extra strikes too
  strikeActivations(setup, counter, strikeNo, strike, strikeEvents, struckEvents);
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
  if (mapStrike) e.flags |= kMapWeapon | (!areaStrike && mapWeaponUses(setup.mapStructure) == 1 ? kMapWeaponSpent : 0);
  if (fightArea_) e.flags |= kArea;
  // 3. Hit. (A hit of 0 still rolls, as it always did.)
  bool hit = f.hit >= 100 || (db->rules.hitRollAverage ? rollHitAverage(f.hit) : rng.chance(f.hit));
  // A "recoil" with "instead" (docs/SRPG.md "Skill blocks, third set"): a strike that lands turns on its wielder, who takes the damage
  // it would have done (`turnedBack`, -1 = nothing turned back); for the foe the strike counts as a miss.
  int turnedBack = -1;
  const bool instead = std::any_of(strike.recoils.begin(), strike.recoils.end(), [](const SkillEffect* r) { return r->instead; });
  if (hit) {
    // 4. Damage: a critical multiplies first, then this strike's percents, then "survive".
    int damage = f.damage;
    if (f.crit > 0 && rng.chance(f.crit)) {
      damage = (int)std::floor(damage * db->rules.critMultiplier);
      e.flags |= kCrit;
    }
    damage = applyPercents(applyPercents(damage, strike.damagePercent), strike.damageTakenPercent);
    Unit& target = units[def];
    if (instead) {
      turnedBack = damage;
      hit = false;
      e.flags &= ~kCrit;
    } else {
      if (fightSpare_ && damage >= target.hp) damage = std::max(0, target.hp - 1);  // a staged fight with "lethal": false
      if (strike.survive && target.hp > 1 && damage >= target.hp) {
        damage = target.hp - 1;
        e.flags |= kSurvived;
      }
      damage = std::min(damage, target.hp);
      target.hp -= damage;
      if (damage > 0 && target.hp > 0) wakeUnit(def);  // a guard that is hit wakes up (docs/AI.md)
      e.d = damage;
      e.flags |= kHit;
      if (target.hp <= 0) e.flags |= kKill;
    }
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
  // 6a. Recoil: the wielder is hurt (it may be defeated: below, with the usual defeat flow). Nothing answers it: it is no strike, so it
  // is never countered, drains nothing and wears nothing. Several recoils each hurt.
  bool wielderDown = false;
  for (const SkillEffect* r : strike.recoils) {
    if (r->instead && turnedBack < 0) continue;  // the strike did not land: nothing to turn back
    if (!units[atk].alive || units[atk].hp <= 0) break;
    SkillCtx rc = skillCombatCtx(setup, atk);
    rc.strikeNo = strikeNo;
    rc.dealt = r->instead ? turnedBack : e.d;
    const int amount = r->expr >= 0 ? (int)std::max(0.0, std::min(1000000.0, std::floor(skillExpr(r->expr, rc, 0)))) : r->value;
    const int loss = std::min(amount, units[atk].hp);
    if (loss <= 0) continue;
    units[atk].hp -= loss;
    Event h;
    h.type = EventType::UnitHurt;
    h.a = atk;
    h.b = loss;
    h.c = atk;
    events.push_back(h);
    wielderDown = wielderDown || units[atk].hp <= 0;
  }
  // 6b. Conditions (docs/SRPG.md section 9), only on a hit: a unit struck loses the conditions that end when it is struck, then
  // the "inflict" effects of the strike's activated blocks take hold (in that order, so a strike that wakes a sleeper can also
  // put it back to sleep): the striker's on the unit struck, the struck unit's on the striker.
  if (hit && db->conditions.size()) {
    if (e.d > 0 && !(e.flags & kKill)) conditionsStruck(def, events);
    for (const SkillEffect* fx : strike.inflictOnStruck)
      if (units[def].alive) inflictCondition(def, fx->conds[0], fx->turns, atk, events);
    for (const SkillEffect* fx : strike.inflictOnStriker)
      if (units[atk].alive) inflictCondition(atk, fx->conds[0], fx->turns, def, events);
  }
  // 7. Wear, drops, defeat.
  if (slot >= 0 && !fightNoWear_) spendUse(atk, slot, events);  // every strike made wears the weapon, hit or miss
  if (mapStrike && !areaStrike) spendMapWeapon(setup.mapStructure, events);  // ... and the structure's weapon likewise
  if (e.flags & kKill) {
    if (fightArea_) pendingKills_.push_back({def, atk, false});  // an area attack lets every victim be struck first
    else defeatStruck(def, atk, events);  // (may run story events: nothing below may hold a reference into `units`)
  }
  if (wielderDown && units[atk].alive) {  // the weapon finished its own wielder: a normal defeat, with no one to credit
    if (fightArea_) {
      pendingKills_.push_back({atk, -1, true});
    } else {
      killUnit(atk, events);
      defeatQuote(atk, events);
      unitDefeated(atk, -1, events);
    }
  }
  // 8. "Strike again": one more strike through the same steps, hit or miss; that one cannot give another.
  if (strike.strikeAgain && !extra && !over && units[atk].alive && units[def].alive)
    doStrike(setup, counter, true, events);
}

// What the fallen unit was marked to drop goes to the one who struck it down.
void Battle::defeatStruck(int def, int killer, std::vector<Event>& events) {
  std::vector<ItemStack> drops;
  for (const ItemStack& s : units[def].items)
    if (s.drop) drops.push_back(s);
  killUnit(def, events, killer);
  for (const ItemStack& s : drops) giveTo(killer, s, kDropped, events);
  defeatQuote(def, events);  // the fallen unit's last words come before the story's "defeated" events
  unitDefeated(def, killer, events);  // may run story events
}

void Battle::takeWeaponInHand(int unit, int weapon) {
  if (!db->rules.items.carriedWeapons) return;
  int slot = weaponSlot(units[unit], weapon);
  if (slot > 0 && slot != equippedSlot(units[unit])) {
    ItemStack s = units[unit].items[(size_t)slot];
    units[unit].items.erase(units[unit].items.begin() + slot);
    units[unit].items.insert(units[unit].items.begin(), s);
  }
}

void Battle::applyAttack(const Action& action, std::vector<Event>& events) {
  if (isAreaWeapon(action.weapon)) {  // area_attack.cpp
    applyAreaAttack(action, events);
    return;
  }
  int a = action.unit, d = action.target;
  Forecast f;
  CombatSetup setup;
  forecastFight(a, d, action.weapon, units[a].cell, f, setup);  // validated by caller

  // "Just before a fight": story events may change things, even take a fighter off the map or end
  // the battle, in which case there is no fight.
  fightQuotes(a, d, events);  // characters' battle quotes come before the "fight" story events (docs/EVENTS.md)
  StoryOccurrence fight;
  fight.kind = Trigger::Kind::Fight;
  fight.unit = a;
  fight.other = d;
  fireStory(fight, events);
  if (over || !units[a].alive || !units[d].alive ||
      !forecastFight(a, d, action.weapon, units[a].cell, f, setup)) {
    if (units[a].alive) endOrder(a, true);
    return;
  }

  takeWeaponInHand(a, action.weapon);  // attacking with a carried weapon takes it in hand
  int hpA = units[a].hp, hpD = units[d].hp;
  fightStrikes_[0] = fightStrikes_[1] = 0;
  fightMarks_.clear();  // marks live for one fight (skill_marks.cpp)
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
      if (!spent && !(setup.mapStructure >= 0 && !step.counter)) spendAmmo(step.counter ? d : a, step.counter ? setup.weaponD : setup.weaponA);
      spent = true;
      doStrike(setup, step.counter, false, events);
    }
  }

  fightExperience(a, d, hpA, hpD, events);
  ranksAfterCombat(events, combatFrom, a, d);  // weapon ranks, growth "use" (docs/SRPG.md section 7); nothing otherwise
  // After the experience: "defeat" / "afterCombat" skills and supports' "fought together" (docs/SRPG.md "Combat
  // pipeline"), and only then the attacker's order ends (a skill may let it act again). A fallen attacker keeps
  // its flags as they were.
  srpgAfterCombat(a, d, hpA, hpD, events);
  fightMarks_.clear();  // the afterCombat blocks were the last to ask
  if (units[a].alive) endOrder(a, true);
}

void Battle::fightExperience(int a, int d, int hpA, int hpD, std::vector<Event>& events, bool attackerOnly) {
  if (!db->rules.progression) return;
  FormulaCtx ctx;
  ctx.sides = &sides;
  ctx.battle = this;
  // Attacker's reward.
  if (units[a].alive && units[d].hp < hpD) {
    ctx.a = &units[a];
    ctx.b = &units[d];
    Formulas::Slot slot = units[d].alive ? Formulas::XpHit : Formulas::XpKill;
    awardXp(a, (int)std::floor(formulas->eval(slot, ctx, 0)), events);
  } else if (units[a].alive && fightStrikes_[0] > 0) {
    awardXp(a, xpNoDamage(a, d), events);  // "xpNoDamage": a fight that did no damage (nothing unless the game sets it)
  }
  if (attackerOnly) return;  // one more victim of an area attack: only the unit on the aimed square may have answered
  // Defender's reward for a successful counter.
  if (units[d].alive && units[a].hp < hpA) {
    ctx.a = &units[d];
    ctx.b = &units[a];
    Formulas::Slot slot = units[a].alive ? Formulas::XpHit : Formulas::XpKill;
    awardXp(d, (int)std::floor(formulas->eval(slot, ctx, 0)), events);
  } else if (units[d].alive && fightStrikes_[1] > 0) {
    awardXp(d, xpNoDamage(d, a), events);  // only a defender that really struck back
  }
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
