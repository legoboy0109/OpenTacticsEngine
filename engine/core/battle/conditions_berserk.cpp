// Berserk (docs/SRPG.md section 9): a unit under a condition with the "berserk" effect acts by itself at the start of its side's
// turn, outside the control of the player or of its own AI, and attacks the nearest unit of ANY side with its equipped weapon.
//
// The choice (berserkPlan), all on what the unit's team sees (the truth decides, sight limits choices):
//  * Candidates are the other units on the board that its team sees (its own side's included; carried units are off the board).
//  * It uses the first weapon it could attack with (the equipped one; with fixed weapons the type's first with ammunition).
//  * Of the candidates it can hit this turn (from a square it can reach, in the weapon's range), it takes the NEAREST to the square it
//    stands on (grid distance); a tie goes to the lower unit id. It stands where it can hit from with the least movement (staying put
//    when that already works); a tie goes to the lower square number (y * width + x).
//  * When nothing is in reach it walks as far as it can towards the nearest candidate (the reachable square nearest to it; ties:
//    the cheaper walk, then the lower square number) and does not attack.
//  * With no weapon, or no target, it stands still. Its turn is then over either way.
// It acts through the same move and attack code as an order does (the same events, the same dice), so a counterattack, experience,
// drops, story events and the win checks all happen as for any fight; only the validity test is its own (it may hit an ally).
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

bool Battle::mayFight(const Unit& a, const Unit& d) const {
  return isEnemy(a, d) || conditionBlock(a, ConditionBlock::Berserk) >= 0;
}

bool Battle::berserkPlan(const Unit& u, BerserkPlan& out) const {
  out = BerserkPlan();
  if (!u.alive || u.cell == kNoCell) return false;
  const int team = teamOf(u);
  struct Candidate {
    int id, dist;
  };
  std::vector<Candidate> others;
  for (const Unit& t : units) {
    if (t.id == u.id || !t.alive || t.cell == kNoCell || !unitVisibleTo(team, t)) continue;
    others.push_back({t.id, grid.distance(u.cell, t.cell)});
  }
  if (others.empty()) return false;
  std::sort(others.begin(), others.end(), [](const Candidate& a, const Candidate& b) { return a.dist != b.dist ? a.dist < b.dist : a.id < b.id; });

  Reach reach;
  reachable(u.id, reach);
  std::vector<Cell> stops = reach.stops;  // a unit that cannot move has only its own square
  if (stops.empty()) stops.push_back(u.cell);
  std::sort(stops.begin(), stops.end(), [&](Cell a, Cell b) {
    const int ca = a == u.cell ? 0 : reach.cost[(size_t)a], cb = b == u.cell ? 0 : reach.cost[(size_t)b];
    return ca != cb ? ca < cb : a < b;
  });

  std::vector<int> weapons;
  weaponsOf(u, weapons);
  if (!weapons.empty()) {
    const int w = weapons[0];
    for (const Candidate& cand : others) {
      const Unit& t = units[(size_t)cand.id];
      double vs;
      if (!versusValue(w, t, vs)) continue;
      for (Cell s : stops) {
        if (s != u.cell && !db->weapons[(size_t)w].afterMove) continue;
        if (!weaponAims(w, s, t.cell, u.id)) continue;
        out.moveTo = s == u.cell ? kNoCell : s;
        out.target = cand.id;
        out.weapon = w;
        return true;
      }
    }
  }
  // Nothing to hit: walk towards the nearest.
  const Unit& near = units[(size_t)others[0].id];
  Cell best = kNoCell;
  int bestDist = grid.distance(u.cell, near.cell);
  for (Cell s : stops) {  // already sorted by cost, then cell: the first of the nearest wins
    const int d = grid.distance(s, near.cell);
    if (d < bestDist) {
      bestDist = d;
      best = s;
    }
  }
  if (best == kNoCell) return false;
  out.moveTo = best;
  return true;
}

void Battle::conditionsAct(std::vector<Event>& events) {
  if (db->conditions.empty()) return;
  for (size_t i = 0; i < units.size() && !over; ++i) {
    Unit& u = units[i];
    if (!ownsTurn(u) || !u.alive || u.cell == kNoCell || u.conditions.empty()) continue;
    if (conditionBlock(u, ConditionBlock::Act) >= 0) continue;  // a stopped unit does nothing (conditionsPrevent)
    const int c = conditionBlock(u, ConditionBlock::Berserk);
    if (c < 0) continue;
    const int id = (int)i;
    events.push_back([&] {
      Event e;
      e.type = EventType::ConditionTick;
      e.a = id;
      e.b = c;
      e.c = kTickActsAlone;
      return e;
    }());
    BerserkPlan plan;
    if (berserkPlan(u, plan)) {
      if (plan.moveTo != kNoCell) applyMove(Action::move(id, plan.moveTo), events);
      // Units may have been added by a story event while it walked: look the unit up again, never keep the reference.
      if (plan.target >= 0 && units[i].alive && units[(size_t)plan.target].alive && !over)
        applyAttack(Action::attack(id, plan.target, plan.weapon), events);
    }
    if (units[i].alive) units[i].done = units[i].moved = true;
    settle(events);  // what the fight decided: defeat rules, objectives
  }
}

}  // namespace ote
