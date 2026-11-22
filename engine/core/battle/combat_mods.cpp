// The combat pipeline's numbers (docs/SRPG.md "Combat pipeline"): who answers with what, the modifiers of both
// fighters, one side's strike numbers and the order of strikes. combat.cpp runs the fight on top of these.
//
// Invariant: with empty bags every function here computes exactly what the formulas alone gave before the
// pipeline existed (tests/test_srpg_golden_fights.cpp pins that).
#include "core/battle/combat_mods.h"

#include <algorithm>
#include <cmath>

#include "core/script/formulas.h"

namespace ote {

// A formula's result as a whole number (rounded down), kept in a range that cannot overflow once modifiers are
// added. The real limits (0..100 percent, 1..8 strikes, ...) are applied after the modifiers.
static int wholeNumber(double v) {
  if (v != v) return 0;  // not a number
  if (v < -kMaxStrikeDamage) return -kMaxStrikeDamage;
  if (v > kMaxStrikeDamage) return kMaxStrikeDamage;
  return (int)std::floor(v);
}

static int clampInt(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

int applyPercents(int value, const std::vector<int>& percents) {
  long long v = value < 0 ? 0 : value;
  for (int p : percents) {
    v = v * (p < 0 ? 0 : p) / 100;
    if (v > kMaxStrikeDamage) v = kMaxStrikeDamage;
  }
  return (int)v;
}

bool rollActivation(Rng& rng, int chance) {
  if (chance <= 0) return false;
  if (chance >= 100) return true;
  return rng.chance(chance);
}

void Battle::combatMods(const CombatSetup& setup, CombatMods& attacker, CombatMods& defender) const {
  attacker = CombatMods();
  defender = CombatMods();
  // The order is part of the rules: percents multiply in the order they are added.
  skillCombatMods(setup, attacker, defender);    // the attacker's own blocks, then the defender's
  auraCombatMods(setup, attacker, defender);     // other units' auras, in unit id order
  supportCombatMods(setup, attacker, defender);  // supports, in pair order
  if (testMods && testMods->combat) testMods->combat(*this, setup, attacker, defender);
}

// Setting up, step 1: the weapon the defender fights with, -1 = none. In range it is the weapon that answers (the
// first such, as always); out of range it is the first that could answer at another distance: still "the weapon
// in use" for its skills, and what a "counterAnyRange" answer strikes with. Whether it answers at all is step 3
// (setUpCombat): this does not look at rules.counterAttacks or, for the fallback, at the range.
int Battle::counterWeapon(const Unit& def, Cell defCell, const Unit& atk, Cell atkCell) const {
  double vs;
  if (db->rules.items.carriedWeapons) {  // only what is in hand answers (docs/ITEMS.md)
    int slot = equippedSlot(def);
    if (slot < 0) return -1;
    int w = itemOf(def.items[(size_t)slot]).weapon;
    return db->weapons[w].counter && versusValue(w, atk, vs) ? w : -1;
  }
  const std::vector<int>& list = typeOf(def).weapons;
  int any = -1;  // the first that could answer at some distance
  for (size_t i = 0; i < list.size(); ++i) {
    int w = list[i];
    if (!def.ammo.empty() && i < def.ammo.size() && def.ammo[i] == 0) continue;  // out of ammunition: the next weapon answers
    if (!db->weapons[w].counter || !versusValue(w, atk, vs) || weaponForbidden(def, w, nullptr)) continue;
    if (weaponAims(w, defCell, atkCell, atk.id)) return w;  // in range (and, for line fire, a clear straight line)
    if (any < 0) any = w;
  }
  return any;
}

// Setting up, steps 1 to 3. Who may fight whom with which weapon is the caller's business (Battle::forecast
// checks it); this only works out the answer.
void Battle::setUpCombat(int attacker, int defender, int weapon, Cell from, CombatSetup& setup, CombatMods& modsA,
                         CombatMods& modsD) const {
  const Unit& a = units[(size_t)attacker];
  const Unit& d = units[(size_t)defender];
  setup = CombatSetup();
  setup.attacker = attacker;
  setup.defender = defender;
  setup.weaponA = weapon;
  setup.from = from;
  // A weapon the unit does not carry that the structure under `from` offers is fired from the structure (docs/SRPG.md "Map weapons").
  setup.mapStructure = mapWeaponUsedBy(a, from, weapon);
  setup.weaponD = counterWeapon(d, d.cell, a, from);
  combatMods(setup, modsA, modsD);
  setup.counters = db->rules.counterAttacks && setup.weaponD >= 0 && !modsA.noCounter &&
                   (modsD.counterAnyRange || weaponAims(setup.weaponD, d.cell, from, attacker)) &&
                   mayCounterWhileStopped(d);  // a unit that cannot act answers nothing unless its condition says it may
}

void Battle::noDodge(const CombatSetup& setup, bool counter, StrikeForecast& f) const {
  const Unit& target = units[(size_t)(counter ? setup.attacker : setup.defender)];
  if (!target.conditions.empty() && !mayDodgeWhileStopped(target)) f.hit = 100;
}

bool Battle::strikeForecast(const CombatSetup& setup, bool counter, const CombatMods& own, const CombatMods& foe,
                            StrikeForecast& out) const {
  out = StrikeForecast();
  const Unit& atk = units[(size_t)(counter ? setup.defender : setup.attacker)];
  const Unit& def = units[(size_t)(counter ? setup.attacker : setup.defender)];
  // The attacker fights from `from` (a forecast: where it would stand); the defender from its own square.
  Cell cellA = setup.from, cellD = units[(size_t)setup.defender].cell;
  Cell atkCell = counter ? cellD : cellA, defCell = counter ? cellA : cellD;
  int weapon = counter ? setup.weaponD : setup.weaponA;
  if (weapon < 0) return false;

  FormulaCtx ctx;
  ctx.sides = &sides;
  ctx.battle = this;  // stats as the rules see them (Battle::stat), plus this combat's bonuses to stats
  if (!versusValue(weapon, def, ctx.vs)) return false;
  ctx.a = &atk;
  ctx.b = &def;
  if (!own.stats.empty()) ctx.bonusA = &own.stats;
  if (!foe.stats.empty()) ctx.bonusB = &foe.stats;
  ctx.followA = own.followShift;
  ctx.followB = foe.followShift;
  ctx.triA = own.trianglePercent;
  ctx.triB = foe.trianglePercent;
  ctx.w = &db->weapons[weapon];
  // The other fighter's weapon: the answering weapon when attacking, the attacker's weapon in a counter (-1 = none).
  const int foeWeapon = counter ? setup.weaponA : setup.weaponD;
  if (foeWeapon >= 0) ctx.fw = &db->weapons[(size_t)foeWeapon];
  // `fw.answers`: the foe's weapon really is used against this strike. In a counter that is the attacker's weapon (the fight is
  // its doing); in the attacker's strike it is the defender's answer, which setUpCombat judged (range, rule, skills): no new pass.
  ctx.fwAnswers = foeWeapon >= 0 && (counter || setup.counters);
  Props scratchA, scratchB;  // outlive the formula calls below
  ctx.ta = &numbersAt(atkCell, scratchA);
  ctx.tb = &numbersAt(defCell, scratchB);
  ctx.ha = heightAt(atkCell);
  ctx.hb = heightAt(defCell);
  // Facing (docs/SRPG.md "Facing"): `arc` = from which side of the struck unit this strike comes. An attacker turns to look at its target before
  // the fight (facing_rules.cpp), so the facing a counter-attack meets is the one toward the defender, not the one the attacker had before.
  if (facingGame()) ctx.arc = arcAt(defCell, counter ? directionToward(grid, defCell, atkCell) : def.facing, atkCell);
  // Damage types (docs/SRPG.md "Damage types"): `rs` is the defender's resistance against this weapon's type; a type that ignores
  // terrain leaves the formulas an empty `tb` (every terrain and structure number of the defender's square) for this strike.
  if (ctx.w->damageType >= 0) {
    ctx.rs = resistOf(def, ctx.w->damageType);
    if (db->damageTypes.list[(size_t)ctx.w->damageType].ignoresTerrain) {
      scratchB.clear();
      ctx.tb = &scratchB;
    }
  }
  ctx.dist = grid.distance(atkCell, defCell);

  out.can = true;
  out.weapon = weapon;
  // Modifiers are added to what the formula gave and the limits are applied to the sum: a hit formula that gives
  // 130 with 20 avoid against it is still 100.
  int damage = wholeNumber(formulas->evalDamage(weapon, ctx)) + own.damage + foe.damageTaken;
  damage = applyPercents(clampInt(damage, 0, kMaxStrikeDamage), own.damagePercent);
  out.damage = applyPercents(damage, foe.damageTakenPercent);
  int hit = formulas->hasHit(weapon) ? wholeNumber(formulas->evalHit(weapon, ctx, 100)) : 100;  // the weapon's own "hit", else the game's
  out.hit = clampInt(hit + own.hit - foe.avoid, 0, 100);
  noDodge(setup, counter, out);
  int crit = formulas->has(Formulas::Crit) ? wholeNumber(formulas->eval(Formulas::Crit, ctx, 0)) : 0;
  out.crit = clampInt(crit + own.crit - foe.critAvoid, 0, 100);
  int strikes = formulas->has(Formulas::Strikes) ? wholeNumber(formulas->eval(Formulas::Strikes, ctx, 1)) : 1;
  out.strikes = clampInt(strikes + own.strikes, 1, 8);
  // Follow-up control: the first exchange is the unit's one strike; anything beyond it is a follow-up. Forbid beats force.
  if (own.followForbid) out.strikes = 1;
  else if (own.followForce && out.strikes < 2) out.strikes = 2;
  out.multi = clampInt(own.multiStrike, 1, 4);
  return true;
}

void Battle::foldStrikeMods(const StrikeMods& mods, StrikeForecast& numbers) const {
  numbers.hit = clampInt(numbers.hit + mods.hit - mods.avoid, 0, 100);
  numbers.crit = clampInt(numbers.crit + mods.crit - mods.critAvoid, 0, 100);
  numbers.damage = clampInt(numbers.damage + mods.damage + mods.damageTaken, 0, kMaxStrikeDamage);
}

void Battle::strikeOrder(const Forecast& forecast, std::vector<StrikeStep>& out) const {
  out.clear();
  if (!forecast.attack.can) return;
  int strikesA = forecast.attack.strikes;
  int strikesD = forecast.counter.can ? forecast.counter.strikes : 0;
  bool defenderFirst = forecast.defenderFirst && forecast.counter.can;
  // One fighter's turn within a round: its `multi` strikes in a row, if it has a strike left in this round.
  auto step = [&](bool counter, int rnd) {
    if (rnd >= (counter ? strikesD : strikesA)) return;
    StrikeStep s;
    s.counter = counter;
    s.count = std::max(1, counter ? forecast.counter.multi : forecast.attack.multi);
    out.push_back(s);
  };
  for (int rnd = 0; rnd < std::max(strikesA, strikesD); ++rnd) {
    step(defenderFirst, rnd);
    step(!defenderFirst, rnd);
  }
}

}  // namespace ote
