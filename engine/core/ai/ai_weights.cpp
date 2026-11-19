// Computer opponent: choosing an attack by a named weight set (docs/AI.md "Attack weights"). A unit whose behaviour names a set of
// rules.ai.weights picks its attack with the score below instead of the default scoring; no randomness anywhere.
//
//   score = max(0, dmgTerm + leftTerm + friendsTerm + classTerm + turnTerm + noCounterBonus - counterTerm - dangerTerm - woundsTerm)
//
// One candidate is a weapon x a target x a square it can attack from; a (weapon, target) pair counts with the score of its best square,
// and equal squares are told apart by the square rule (see better()). The pair with the highest score wins; ties go to the LATER pair in
// the order weapon (the unit's own order) outer, target (unit id) inner.
#include <algorithm>
#include <map>
#include <memory>

#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

namespace {

constexpr double kWeightedBase = 200;  // as the default attack scores: any attack outranks plain advancing

// What one side of a fight is expected to deal: damage x hit% / 100 for every strike it makes (no critical hits), not capped.
double predicted(const StrikeForecast& f) {
  if (!f.can) return 0;
  double sum = 0;
  if (!f.each.empty()) {
    for (const StrikeNumbers& n : f.each) sum += n.damage * n.hit / 100.0;
    return sum;
  }
  return f.damage * (f.hit / 100.0) * f.strikes * std::max(1, f.multi);
}

double terrainAvoid(const Battle& b, Cell c) {
  double d = 0;
  Props scratch;
  propFind(b.numbersAt(c, scratch), "avoid", d);
  return d;
}

int adjacentEnemies(const Battle& b, const Unit& u, Cell c) {
  int n = 0;
  for (const Unit& o : b.units)
    if (o.alive && b.isEnemy(u, o) && knownEnemy(b, u.side, o) && b.grid.distance(c, o.cell) == 1) ++n;
  return n;
}

// The damage the unit could take at `sq` from the known enemies that can reach it: for each such enemy, the most it expects to deal to the
// unit there with any of its weapons from any square it can walk to (capped at the unit's hit points), added up over the enemies. Worked out on
// a copy of the battle with the unit moved to `sq`, so terrain and blocking are those of the square. Enemy flags of last turn are cleared.
double dangerAt(const Battle& b, const Unit& u, Cell sq, std::unique_ptr<Battle>& scratch) {
  // Only enemies the unit's side knows of count; with none, nothing below would run, so skip the copy altogether.
  bool any = false;
  for (const Unit& e : b.units)
    if (e.alive && b.isEnemy(u, e) && knownEnemy(b, u.side, e)) { any = true; break; }
  if (!any) return 0;
  // A fresh copy of the battle each time (not one reused and moved about): the copy carries b's mutable caches (sight, fight
  // marks) and the queries below may leave state in them, so reusing it could change a later answer. Assigning into the one
  // `scratch` keeps its vectors' memory, which is most of what a new copy costs. No random numbers are drawn by any of this.
  if (!scratch) scratch.reset(new Battle(b));
  else *scratch = b;
  Battle& sim = *scratch;
  Unit& me = sim.units[(size_t)u.id];
  sim.occupant[(size_t)me.cell] = -1;
  me.cell = sq;
  sim.occupant[(size_t)sq] = me.id;
  double sum = 0;
  std::vector<AttackOption> options;
  for (Unit& e : sim.units) {
    if (!e.alive || !sim.isEnemy(me, e) || !knownEnemy(b, u.side, b.units[(size_t)e.id])) continue;
    e.moved = e.acted = false;
    e.moveSpent = 0;
    Reach reach;
    sim.reachable(e.id, reach);
    double best = 0;
    for (Cell s : reach.stops) {
      sim.attackOptions(e.id, s, options);
      for (const AttackOption& o : options) {
        if (o.target != me.id) continue;
        Forecast f;
        if (sim.forecast(e.id, me.id, o.weapon, s, f)) best = std::max(best, predicted(f.attack));
      }
    }
    sum += std::min(best, (double)me.hp);
  }
  return sum;
}

struct Cand {
  int weaponRank = 0, weapon = -1, target = -1;
  double score = 0;
  Cell square = kNoCell;
  int travel = 0;
  bool cannotAnswer = false;
  double defense = 0, avoid = 0;
  int adjacent = 0;
  bool have = false;
};

// Is the square of `a` a better place to make the same attack than the one of `c`? (Only asked between squares of the same score.)
// Order: the target cannot answer; better terrain defence; better avoid; fewer adjacent enemies; lower path cost; lower square number.
bool better(const Cand& a, const Cand& c) {
  if (a.cannotAnswer != c.cannotAnswer) return a.cannotAnswer;
  if (a.defense != c.defense) return a.defense > c.defense;
  if (a.avoid != c.avoid) return a.avoid > c.avoid;
  if (a.adjacent != c.adjacent) return a.adjacent < c.adjacent;
  if (a.travel != c.travel) return a.travel < c.travel;
  return a.square < c.square;
}

}  // namespace

bool weightedAttack(const Battle& b, const Unit& u, const Stance& st, const Reach& reach, const std::vector<Cell>& stops, Order& out) {
  const AiWeightSet& w = b.db->rules.ai.weights[(size_t)u.behaviour.weightSet];
  std::vector<int> ownWeapons;
  b.weaponsOf(u, ownWeapons);
  std::vector<Cand> cands;
  std::map<Cell, double> dangerCache;
  std::unique_ptr<Battle> dangerScratch;  // storage for dangerAt's copy of the battle, shared by all squares of this call
  std::vector<AttackOption> options;
  const int missingPct = 100 - u.hp * 100 / std::max(1, b.maxHp(u));

  for (Cell s : stops) {
    const int travel = s == u.cell ? 0 : reach.cost[(size_t)s];
    b.attackOptions(u.id, s, options);
    for (const AttackOption& o : options) {
      if (o.mapWeapon && s != u.cell && !(u.aiGoals & kAiWeapons)) continue;  // a free map weapon only for a unit marked "goesFor": ["weapons"]
      if (ignoredTarget(st, o.target)) continue;
      const Unit& target = b.units[(size_t)o.target];
      Forecast f;
      if (!b.forecast(u.id, o.target, o.weapon, s, f)) continue;

      const double pred = predicted(f.attack);
      const double dealt = std::min(pred, (double)target.hp);
      const bool kill = f.attack.can && target.hp > 0 && pred >= target.hp;
      double score = kill ? w.killValue : std::min(dealt, (double)w.dmgCap) * w.dmg;  // dmgTerm
      score += std::max(0.0, w.leftBase - (target.hp - dealt)) * w.left;                // leftTerm
      int friends = 0;
      for (const Unit& a : b.units)
        if (a.alive && a.id != u.id && !b.isEnemy(u, a) && b.grid.distance(a.cell, target.cell) == 1) ++friends;
      score += std::min((double)w.friendsCap, (double)friends * w.friends);              // friendsTerm
      score += (double)w.cls * w.priorityOf(target.type);                                // classTerm
      score += (double)b.round * w.turn;                                                 // turnTerm
      const bool cannotAnswer = !f.counter.can;
      if (cannotAnswer) score += w.noCounterBonus;
      else if (!kill) score -= std::min(std::min(predicted(f.counter), (double)u.hp), (double)w.counterCap) * w.counter;  // counterTerm
      if (w.danger > 0) {
        auto it = dangerCache.find(s);
        if (it == dangerCache.end()) it = dangerCache.insert(std::make_pair(s, dangerAt(b, u, s, dangerScratch))).first;
        score -= std::min((double)w.dangerCap, w.danger * it->second / w.dangerDiv);     // dangerTerm
      }
      score -= w.wounds * missingPct / 5.0;                                              // woundsTerm
      score = std::max(0.0, score);

      Cand c;
      c.weapon = o.weapon;
      c.target = o.target;
      c.score = score;
      c.square = s;
      c.travel = travel;
      c.cannotAnswer = cannotAnswer;
      c.defense = terrainDefense(b, s);
      c.avoid = terrainAvoid(b, s);
      c.adjacent = adjacentEnemies(b, u, s);
      c.have = true;
      size_t rank = std::find(ownWeapons.begin(), ownWeapons.end(), o.weapon) - ownWeapons.begin();
      c.weaponRank = (int)rank;
      bool merged = false;
      for (Cand& e : cands) {
        if (e.weapon != c.weapon || e.target != c.target) continue;
        merged = true;
        if (c.score > e.score || (c.score == e.score && better(c, e))) {
          c.weaponRank = e.weaponRank;
          e = c;
        }
        break;
      }
      if (!merged) cands.push_back(c);
    }
  }

  // Weapons outside the unit's own list (a map weapon) come after it, by weapon number.
  std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& c) {
    if (a.weaponRank != c.weaponRank) return a.weaponRank < c.weaponRank;
    if (a.weapon != c.weapon) return a.weapon < c.weapon;
    return a.target < c.target;
  });
  const Cand* chosen = nullptr;
  for (const Cand& c : cands) {
    if (c.score <= 0 && !w.attackAtZero) continue;
    if (!chosen || c.score >= chosen->score) chosen = &c;  // >=: the later one wins a tie
  }
  if (!chosen) return false;
  out = Order();
  out.score = kWeightedBase + chosen->score;
  out.kind = ActionType::Attack;
  out.moveTo = chosen->square == u.cell ? kNoCell : chosen->square;
  out.target = chosen->target;
  out.weapon = chosen->weapon;
  return true;
}

}  // namespace ai
}  // namespace ote
