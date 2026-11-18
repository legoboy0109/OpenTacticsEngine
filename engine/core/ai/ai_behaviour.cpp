// Computer opponent: per-unit behaviour (docs/AI.md). A unit whose behaviour is the default never reaches any of this beyond one
// test, so the plain AI's decisions are exactly what they were.
//
//   stanceFor      turns the unit's behaviour into a Stance for this decision: idle (does nothing), stay (never changes square),
//                  noAdvance (moves only to act, never to approach), or seeking (heads for a goal instead of the nearest enemy).
//   updateWakes    at the start of every AI step: a sleeping guard that has a reason wakes (and its group with it).
//   adjustAttack   the attack filter / nudge of a seeker and of a unit with a favourite target.
//   considerSeek   the walk toward a seeker's goal (the counterpart of considerAdvance).
#include <algorithm>

#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

namespace {

constexpr double kPreferNudge = 0.25;  // a favourite target's attack score grows by at most this share: a tie-breaker, not an override
constexpr double kSeekTargetFactor = 4, kSeekTargetBonus = 1000;  // attacking the unit a seeker is after outweighs any other hit

bool hostileKnown(const Battle& b, const Unit& u, const Unit& o) { return o.alive && b.isEnemy(u, o) && knownEnemy(b, u.side, o); }

// The unit's "threat range": could it hit anything it knows of from some square it can reach this turn?
bool couldStrike(const Battle& b, const Unit& u) {
  Reach reach;
  b.reachable(u.id, reach);
  std::vector<AttackOption> options;
  for (Cell s : reach.stops) {
    b.attackOptions(u.id, s, options);
    for (const AttackOption& o : options)
      if (!(o.mapWeapon && s != u.cell && !(u.aiGoals & kAiWeapons))) return true;  // (a free ballista counts only for a unit that goes for one)
  }
  return false;
}

bool inArea(const Battle& b, const AiBehaviour& beh, Cell c) {
  const int x = b.grid.x(c), y = b.grid.y(c);
  return x >= beh.ax && y >= beh.ay && x < beh.ax + beh.aw && y < beh.ay + beh.ah;
}

bool shouldWake(const Battle& b, const Unit& u) {
  const AiBehaviour& beh = u.behaviour;
  if (beh.mode == AiMode::Guard) return couldStrike(b, u);
  if (beh.within > 0 && beh.areaMoveCost && hostileWithinMoveCost(b, u, beh.within)) return true;  // "measure": "moveCost"
  for (const Unit& o : b.units) {
    if (!hostileKnown(b, u, o)) continue;
    if (beh.within > 0 && !beh.areaMoveCost && b.grid.distance(u.cell, o.cell) <= beh.within) return true;
    if (beh.hasArea && inArea(b, beh, o.cell)) return true;
  }
  return false;
}

// The goal squares of a seeker, or false when it has none right now (the unit is gone, or a hostile one it cannot see).
bool seekGoals(const Battle& b, const Unit& u, std::vector<Cell>& goals, int& targetUnit) {
  const AiBehaviour& beh = u.behaviour;
  goals.clear();
  targetUnit = -1;
  if (!beh.seekUnit.empty()) {
    const int t = b.unitByStoryId(beh.seekUnit);
    if (t < 0 || !b.units[(size_t)t].alive || t == u.id) return false;
    const Unit& target = b.units[(size_t)t];
    if (b.isEnemy(u, target) && !knownEnemy(b, u.side, target)) return false;  // fog: it does not know where that unit is
    goals.push_back(target.cell);
    targetUnit = t;
    return true;
  }
  if (!beh.seekPlace.empty()) {
    const int s = b.placeByStoryId(beh.seekPlace);
    if (s < 0) return false;
    b.structureCells(s, goals);
    return !goals.empty();
  }
  if (beh.seekCell) {
    if (beh.sx >= b.grid.width() || beh.sy >= b.grid.height()) return false;
    goals.push_back(b.grid.at(beh.sx, beh.sy));
    return true;
  }
  return false;
}

}  // namespace

void updateWakes(Battle& b) {
  const int side = b.activeSide;
  for (size_t i = 0; i < b.units.size(); ++i) {
    const Unit& u = b.units[i];
    if (!u.alive || u.side != side || u.awake || u.done) continue;
    const AiBehaviour& beh = u.behaviour;
    const bool sleeper = (beh.mode == AiMode::Guard && !beh.react) || beh.mode == AiMode::GuardArea;
    if (!sleeper || b.round < beh.fromTurn) continue;
    if (shouldWake(b, u)) b.wakeUnit((int)i);  // wakes its group too
  }
}

Stance stanceFor(Context& cx, const Unit& u, const std::vector<Cell>& stops) {
  Stance st;
  const AiBehaviour& beh = u.behaviour;
  if (beh.isDefault()) return st;
  const Battle& b = cx.b;
  st.beh = &beh;
  if (b.round < beh.fromTurn) {
    st.idle = true;
    return st;
  }
  switch (beh.mode) {
    case AiMode::Advance:
      break;
    case AiMode::Hold:
      st.stay = st.noAdvance = true;
      break;
    case AiMode::Guard:
      if (beh.react) st.noAdvance = true;  // never wakes for good: strikes what it can reach, never approaches
      else if (!u.awake) st.idle = true;
      break;
    case AiMode::GuardArea:
      if (!u.awake) st.idle = true;
      break;
    case AiMode::Patrol:
      // it attacks what it can reach like a reactive guard (nothing to wake); its own walk is considerPatrol's, not the advance
      fillPatrol(b, u, st);
      break;
    case AiMode::Seek: {
      std::vector<Cell> goals;
      if (!seekGoals(b, u, goals, st.seekTarget)) break;  // nothing to head for: acts as Advance
      distanceField(b, b.typeOf(u).moveType, goals, st.field);
      st.seeking = true;
      st.bestGoal = kFar;
      for (Cell s : stops) st.bestGoal = std::min(st.bestGoal, st.field[(size_t)s]);
      break;
    }
  }
  if (beh.idleTurns > 0 && u.idleSeen <= beh.idleTurns) st.noAdvance = true;  // "idleTurns": it attacks what it can reach but makes no approach move
  fillIgnores(b, u, st);
  return st;
}

bool adjustAttack(const Battle& b, const Stance& st, const Unit& u, const Unit& target, Cell from, double& score) {
  if (!st.beh) return true;
  if (ignoredTarget(st, target.id) && target.id != st.seekTarget) return false;  // "ignores" (a seeker's own target is exempt)
  if (st.seeking) {
    if (target.id == st.seekTarget) score = score * kSeekTargetFactor + kSeekTargetBonus;
    else if (st.field[(size_t)from] > st.bestGoal) return false;  // a free hit only: it must not cost the seeker ground
  }
  const AiBehaviour& beh = *st.beh;
  double share = 0;
  switch (beh.prefer) {
    case AiPrefer::None: break;
    case AiPrefer::Weakest: share = 1.0 - target.hp / (double)std::max(1, b.maxHp(target)); break;
    case AiPrefer::Nearest: share = 1.0 / std::max(1, b.grid.distance(u.cell, target.cell)); break;
    case AiPrefer::Unit: share = !target.storyId.empty() && target.storyId == beh.preferText ? 1 : 0; break;
    case AiPrefer::Type: share = target.type == beh.preferType ? 1 : 0; break;
    case AiPrefer::Tag:
      for (const std::string& t : b.typeOf(target).tags)
        if (t == beh.preferText) share = 1;
      break;
  }
  score *= 1 + kPreferNudge * share;
  return true;
}

void considerSeek(const Stance& st, Context& cx, const Unit& u, const std::vector<Cell>& stops, Order& best) {
  const Battle& b = cx.b;
  if (!st.seeking || stops.size() < 2) return;
  const std::vector<int>& field = st.field;
  int bestVal = field[(size_t)u.cell];
  Cell bestCell = kNoCell;
  double bestDef = -1;
  for (Cell s : stops) {
    if (s == u.cell || field[(size_t)s] >= kFar) continue;
    const int val = field[(size_t)s];
    const double def = terrainDefense(b, s);
    if (val < bestVal || (val == bestVal && bestCell != kNoCell && def > bestDef)) {
      bestVal = val;
      bestCell = s;
      bestDef = def;
    }
  }
  if (bestCell == kNoCell) return;  // nothing closer than where it stands
  const double score = 100 - std::min(bestVal, 99);
  if (score > best.score) {
    best = Order();
    best.score = score;
    best.moveTo = bestCell;
    best.kind = ActionType::Wait;
  }
}

}  // namespace ai
}  // namespace ote
