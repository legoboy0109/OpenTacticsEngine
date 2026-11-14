// Combat: targeting, forecasts and resolving an Attack action.
#include <algorithm>
#include <cmath>

#include "core/battle/battle.h"
#include "core/script/formulas.h"

namespace ote {

static int clampInt(double v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return (int)std::floor(v);
}

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

bool Battle::strikeForecast(const Unit& atk, Cell atkCell, const Unit& def, Cell defCell,
                            int weapon, StrikeForecast& out) const {
  out = StrikeForecast();
  FormulaCtx ctx;
  if (!versusValue(weapon, def, ctx.vs)) return false;
  ctx.a = &atk;
  ctx.b = &def;
  ctx.w = &db->weapons[weapon];
  Props scratchA, scratchB;  // outlive the formula calls below
  ctx.ta = &numbersAt(atkCell, scratchA);
  ctx.tb = &numbersAt(defCell, scratchB);
  ctx.dist = grid.distance(atkCell, defCell);

  out.can = true;
  out.weapon = weapon;
  out.damage = clampInt(formulas->evalDamage(weapon, ctx), 0, 1000000);
  out.hit = formulas->has(Formulas::Hit) ? clampInt(formulas->eval(Formulas::Hit, ctx, 100), 0, 100) : 100;
  out.crit = formulas->has(Formulas::Crit) ? clampInt(formulas->eval(Formulas::Crit, ctx, 0), 0, 100) : 0;
  out.strikes = formulas->has(Formulas::Strikes) ? clampInt(formulas->eval(Formulas::Strikes, ctx, 1), 1, 8) : 1;
  return true;
}

// First weapon the defender can answer with, or -1.
int Battle::counterWeapon(const Unit& def, Cell defCell, const Unit& atk, Cell atkCell) const {
  if (!db->rules.counterAttacks) return -1;
  int dist = grid.distance(defCell, atkCell);
  for (int w : typeOf(def).weapons) {
    double vs;
    if (db->weapons[w].counter && weaponInRange(w, dist) && versusValue(w, atk, vs)) return w;
  }
  return -1;
}

void Battle::attackOptions(int unit, Cell from, std::vector<AttackOption>& out) const {
  out.clear();
  if (unit < 0 || unit >= (int)units.size() || !units[unit].alive || !grid.valid(from)) return;
  const Unit& u = units[unit];
  bool moved = u.moved || from != u.cell;
  for (int w : typeOf(u).weapons) {
    if (moved && !db->weapons[w].afterMove) continue;
    for (const Unit& t : units) {
      if (!t.alive || !isEnemy(u, t)) continue;
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
  out = Forecast();
  if (attacker < 0 || attacker >= (int)units.size() || defender < 0 ||
      defender >= (int)units.size() || !grid.valid(from))
    return false;
  const Unit& a = units[attacker];
  const Unit& d = units[defender];
  if (!a.alive || !d.alive || !isEnemy(a, d)) return false;
  const std::vector<int>& owned = typeOf(a).weapons;
  if (std::find(owned.begin(), owned.end(), weapon) == owned.end()) return false;
  if ((a.moved || from != a.cell) && !db->weapons[weapon].afterMove) return false;
  if (!weaponInRange(weapon, grid.distance(from, d.cell))) return false;
  if (!strikeForecast(a, from, d, d.cell, weapon, out.attack)) return false;
  int cw = counterWeapon(d, d.cell, a, from);
  if (cw >= 0) strikeForecast(d, d.cell, a, from, cw, out.counter);
  return true;
}

void Battle::doStrike(int atk, int def, int weapon, int baseFlags, std::vector<Event>& events) {
  // Recomputed per strike on purpose: earlier strikes change hp, and many
  // formulas (Advance Wars style) scale damage with current hp.
  StrikeForecast f;
  if (!strikeForecast(units[atk], units[atk].cell, units[def], units[def].cell, weapon, f)) return;

  Event e;
  e.type = EventType::Attack;
  e.a = atk;
  e.b = def;
  e.c = weapon;
  e.d = 0;
  e.flags = baseFlags;
  bool hit = f.hit >= 100 || rng.chance(f.hit);
  if (hit) {
    int damage = f.damage;
    if (f.crit > 0 && rng.chance(f.crit)) {
      damage = (int)std::floor(damage * db->rules.critMultiplier);
      e.flags |= kCrit;
    }
    Unit& target = units[def];
    damage = std::min(damage, target.hp);
    target.hp -= damage;
    e.d = damage;
    e.flags |= kHit;
    if (target.hp <= 0) e.flags |= kKill;
  }
  events.push_back(e);
  if (e.flags & kKill) killUnit(def, events);
}

void Battle::applyAttack(const Action& action, std::vector<Event>& events) {
  int a = action.unit, d = action.target;
  Forecast f;
  forecast(a, d, action.weapon, units[a].cell, f);  // validated by caller

  int hpA = units[a].hp, hpD = units[d].hp;
  int rounds = std::max(f.attack.strikes, f.counter.can ? f.counter.strikes : 0);
  for (int i = 0; i < rounds; ++i) {
    if (i < f.attack.strikes && units[a].alive && units[d].alive)
      doStrike(a, d, action.weapon, 0, events);
    if (f.counter.can && i < f.counter.strikes && units[a].alive && units[d].alive)
      doStrike(d, a, f.counter.weapon, kCounter, events);
  }
  if (units[a].alive) units[a].done = true;

  if (db->rules.progression) {
    FormulaCtx ctx;
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
}

void Battle::killUnit(int unit, std::vector<Event>& events) {
  Unit& u = units[unit];
  if (!u.alive) return;
  releaseCapture(unit, kNoCell);  // a dead capturer abandons the capture
  Event e;
  e.type = EventType::UnitDied;
  e.a = unit;
  e.b = u.cell;
  events.push_back(e);
  if (u.cell != kNoCell) occupant[u.cell] = -1;
  u.alive = false;
  u.hp = 0;
  u.cell = kNoCell;
  if (u.leader) sides[u.side].leaderLost = true;
}

}  // namespace ote
