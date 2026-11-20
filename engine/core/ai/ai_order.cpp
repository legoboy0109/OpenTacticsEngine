// Picks the best order (move + attack/capture/wait) for a single unit.
#include <algorithm>

#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

namespace {

// Any attack that deals damage must outrank plain advancing, whatever its
// (possibly tiny) positive score; advancing scores stay below this.
constexpr double kAttackBase = 200;
constexpr double kMoveCostWeight = 0.5;       // prefer shorter trips between equal attacks
constexpr double kCaptureCostWeight = 20;     // "prefer nearer" capture targets
constexpr int kDangerCost = 5;                // advance cost units per outnumbering enemy
constexpr int kSupportRadius = 2;             // friends this close count as support
constexpr int kFadeStart = 6, kFadeLength = 6;  // danger weight falls to 0 over these rounds
constexpr double kExposure = 0.12;        // attack score lost per outnumbering enemy, x unit value
constexpr double kLeaderExposure = 3000;  // same, flat, for a leader whose death loses the game
constexpr int kTooClosePenalty = 6;           // advance cost units, see closePenalty()

// Artillery-like units (every weapon has a minimum range) gain nothing from
// walking next to an enemy: they cannot shoot it there. Make such cells
// less attractive when advancing.
int closePenalty(const Battle& b, const Unit& u, Cell c, int minRange) {
  if (minRange <= 1) return 0;
  for (const Unit& e : b.units)
    if (knownEnemy(b, u.side, e) && b.grid.distance(c, e.cell) < minRange) return kTooClosePenalty;
  return 0;
}

// How outnumbered `u` would be on `c` at the start of the enemy's turn:
// enemies that could reach and hit it minus its own side's units on and
// close to the cell, never below 0 (an even fight is acceptable). Stops lone units from sprinting into the
// enemy army (the classic AI blunder), while a grouped advance is free.
// The effect fades after a few rounds so two cautious armies cannot
// stall each other forever.
double fadeFactor(const Battle& b) {
  double fade = b.round <= kFadeStart ? 1.0 : 1.0 - (b.round - kFadeStart) / (double)kFadeLength;
  return fade > 0 ? fade : 0;
}

int outnumbered(const Battle& b, const Unit& u, Cell c) {
  int threats = 0, support = 0;
  for (const Unit& o : b.units) {
    if (!o.alive || o.id == u.id) continue;
    int dist = b.grid.distance(c, o.cell);
    if (b.isEnemy(u, o)) {
      if (!knownEnemy(b, u.side, o)) continue;  // fog: an enemy it cannot see is not counted
      int range = 1;
      std::vector<int> weapons;
      b.weaponsOf(o, weapons);
      for (int w : weapons) range = std::max(range, b.db->weapons[w].maxRange);
      if (dist <= b.movePoints(o) + range) ++threats;
    } else if (dist <= kSupportRadius) {
      ++support;
    }
  }
  return std::max(0, threats - support - 1);  // the unit itself counts as one of the defenders
}

// A leader whose death loses the game stays home while it has company;
// it only walks forward when it is the last unit standing.
bool leaderStaysHome(const Battle& b, const Unit& u) {
  if (!u.leader || !b.db->rules.defeatLeaderDead) return false;
  for (const Unit& o : b.units)
    if (o.alive && o.side == u.side && o.id != u.id && !o.leader) return true;
  return false;
}

int danger(const Battle& b, const Unit& u, Cell c, double fade) {
  return (int)(outnumbered(b, u, c) * kDangerCost * fade);
}

void considerAdvance(Context& cx, const Unit& u, const Stance& st,
                     const std::vector<Cell>& stops, Order& best) {
  const Battle& b = cx.b;
  if (stops.size() < 2 || leaderStaysHome(b, u)) return;

  // Capturers walk to their assigned city; everyone else toward the nearest enemy.
  const std::vector<int>* field = nullptr;
  if (b.typeOf(u).canCapture && u.id < (int)cx.assigned.size() && cx.assigned[u.id] != kNoCell)
    field = &cx.goalFieldFor(u, cx.assigned[u.id]);
  else if (st.ownGoals)
    field = &st.goalField;  // "ignores": the nearest hostile unit it does not ignore
  else
    field = &cx.enemyFieldFor(u);  // (height rules: around the cliffs the unit cannot climb)

  // "caution": "none" (docs/AI.md): straight-line pursuit, the outnumbered / danger cost of a square does not count.
  const bool cautious = !(st.beh && st.beh->cautionNone);
  const double fade = cautious ? fadeFactor(b) : 0;
  int minRange = kFar;
  std::vector<int> weapons;
  b.weaponsOf(u, weapons);
  for (int w : weapons) minRange = std::min(minRange, b.db->weapons[w].minRange);
  if (weapons.empty()) minRange = 1;

  int here = (*field)[u.cell] >= kFar ? kFar
      : (*field)[u.cell] + closePenalty(b, u, u.cell, minRange) + danger(b, u, u.cell, fade);
  Cell bestCell = kNoCell;
  int bestVal = here;
  double bestDef = -1;
  for (Cell s : stops) {
    if (s == u.cell || (*field)[s] >= kFar) continue;
    int val = (*field)[s] + closePenalty(b, u, s, minRange) + danger(b, u, s, fade);
    double def = terrainDefense(b, s) + (b.heightGame() ? 0.01 * b.heightAt(s) : 0);  // (height rules: of equal squares the higher one)
    if (val < bestVal || (val == bestVal && bestCell != kNoCell && def > bestDef)) {
      bestVal = val;
      bestCell = s;
      bestDef = def;
    }
  }
  if (bestCell == kNoCell) return;  // nothing closer than where we stand: do not shuffle

  // Front-line units (small remaining distance) go first so they clear the
  // way for those behind them.
  double score = 100 - std::min(bestVal, 99);
  if (score > best.score) {
    best = Order();
    best.score = score;
    best.moveTo = bestCell;
    best.kind = ActionType::Wait;
  }
}

}  // namespace

double exposureCost(const Battle& b, const Unit& u, Cell c) {
  const double fade = fadeFactor(b);
  double cost = outnumbered(b, u, c) * kExposure * unitValue(b, u) * fade * (0.5 + 0.5 * u.hp / (double)std::max(1, b.maxHp(u)));
  if (u.leader && b.db->rules.defeatLeaderDead) cost += outnumbered(b, u, c) * kLeaderExposure * fade;
  return cost;
}

Order bestOrder(Context& cx, const Unit& u) {
  const Battle& b = cx.b;
  Reach reach;
  b.reachable(u.id, reach);
  std::vector<Cell> stops;
  if (u.moved && !u.acted) stops.push_back(u.cell);  // already moved (human handover): cannot move again
  else stops = reach.stops;  // (an acted unit's reach is what its "move again" has left)
  const Stance st = stanceFor(cx, u, stops);          // docs/AI.md: the default behaviour gives an empty one
  if (st.idle || (st.stay && u.acted)) return Order();
  if (st.stay) stops.assign(1, u.cell);                // hold: acts from where it stands
  if (u.acted) return moveAgainOrder(b, u, stops);   // action used up: only the rest of its movement
  if (u.retreating && st.beh && st.beh->retreat.on) return retreatOrder(cx, u, stops);  // docs/AI.md "Retreat to heal": no attacking
  // docs/AI.md "Attack weights": the unit's own set, else the one its side's faction brings (docs/AI.md "Factions"), else the default scoring.
  const AiWeightSet* weights = st.beh && st.beh->weightSet >= 0 ? &b.db->rules.ai.weights[(size_t)st.beh->weightSet] : b.factionWeights(u.side);

  const int healKey = st.beh && st.beh->healsRetreating ? lastRetreatingHeal(b, u, stops) : kHealAnyone;  // docs/AI.md "heals"
  const double fade = fadeFactor(b);
  Order best;  // default: Wait in place, score 0
  Order viaMapWeapon;  // the best attack from a free map weapon it would walk to (only a unit marked "goesFor": ["weapons"]); a fallback
  std::vector<AttackOption> options;
  for (Cell s : stops) {
    int travel = s == u.cell ? 0 : reach.cost[s];

    if (knownCapturable(b, u, s) && cx.mayCapture(u, s)) {
      double sc = scoreCapture(b, u, s) - kCaptureCostWeight * travel;
      if (sc > best.score) {
        best = Order();
        best.score = sc;
        best.kind = ActionType::Capture;
        best.moveTo = s == u.cell ? kNoCell : s;
      }
    }

    considerStaves(b, u, s, travel, best, healKey);  // a staff beats an attack only by its own worth (ai_staves.cpp)

    if (weights) options.clear();  // it chooses its attack by its weights, below
    else {
      b.attackOptions(u.id, s, options);
      dropCharged(b, options);  // a charged weapon is judged by ai_speed.cpp
    }
    for (const AttackOption& o : options) {
      const bool walkToWeapon = o.mapWeapon && s != u.cell;  // docs/SRPG.md "Map weapons": only a marked unit goes for one
      if (walkToWeapon && !(u.aiGoals & kAiWeapons)) continue;
      double sc = o.area ? scoreAreaAttack(b, u, o.weapon, o.cell, s) : scoreAttack(b, u, b.units[o.target], o.weapon, s);
      if (sc <= 0) continue;
      sc -= outnumbered(b, u, s) * kExposure * unitValue(b, u) * fade *
            (0.5 + 0.5 * u.hp / (double)std::max(1, b.maxHp(u)));
      if (u.leader && b.db->rules.defeatLeaderDead) sc -= outnumbered(b, u, s) * kLeaderExposure * fade;
      // (an area attack on an empty square, or on one of its own, has no target for the stance's likes and dislikes to judge)
      const bool judged = o.target >= 0 && b.isEnemy(u, b.units[o.target]);
      if (sc <= 0 || (judged && !adjustAttack(b, st, u, b.units[o.target], s, sc))) continue;
      sc += kAttackBase - kMoveCostWeight * travel;
      if (walkToWeapon) {
        if (sc > viaMapWeapon.score) {
          viaMapWeapon = Order();
          viaMapWeapon.score = sc;
          viaMapWeapon.kind = ActionType::Attack;
          viaMapWeapon.moveTo = s;
          viaMapWeapon.target = o.target;
          viaMapWeapon.weapon = o.weapon;
          viaMapWeapon.cell = o.area ? o.cell : kNoCell;
        }
        continue;
      }
      if (sc > best.score) {
        best = Order();
        best.score = sc;
        best.kind = ActionType::Attack;
        best.moveTo = s == u.cell ? kNoCell : s;
        best.target = o.target;
        best.weapon = o.weapon;
        best.cell = o.area ? o.cell : kNoCell;  // an area weapon says which square it aims at
      }
    }
  }

  if (weights) {
    Order w;
    if (weightedAttack(b, u, *weights, st, reach, stops, w) && w.score > best.score) best = w;
  }

  // No attack of its own (a capture or a staff may be the best order): a ballista it can reach and fire from beats walking on.
  if (viaMapWeapon.kind == ActionType::Attack && best.kind != ActionType::Attack && viaMapWeapon.score > best.score) best = viaMapWeapon;

  if (!u.moved && !st.noAdvance) {
    if (st.seeking) considerSeek(st, cx, u, stops, best);
    else if (st.patrolling) considerPatrol(st, b, u, stops, best);
    else if (u.behaviour.mode != AiMode::Patrol) considerAdvance(cx, u, st, stops, best);
  }
  considerPlaces(cx, u, reach, stops, best);  // chests, villages and doors, for a unit the map marked (ai_places.cpp)
  considerHealing(b, u, best);
  considerCuring(b, u, best);
  considerSrpg(b, u, best);
  considerSupply(b, u, reach, stops, best);
  return best;
}

}  // namespace ai
}  // namespace ote
