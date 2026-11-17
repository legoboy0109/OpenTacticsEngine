// Turn flow, income and defeat conditions.
#include "core/battle/battle.h"

namespace ote {

void Battle::changeResource(int side, int resource, int delta, std::vector<Event>& events) {
  if (delta == 0) return;
  sides[side].resources[resource] += delta;
  Event e;
  e.type = EventType::ResourceChanged;
  e.a = side;
  e.b = resource;
  e.c = delta;
  e.d = sides[side].resources[resource];
  events.push_back(e);
}

// A light lasts for `turns` of its side's turns: each time that side's turn starts it has one fewer, and at 0 it is gone
// (so the default 1 = until that side's next turn starts). Dropped from the sight cache, which is derived data.
void Battle::lightsTurnStart() {
  bool changed = false;
  for (size_t i = lights.size(); i-- > 0;)
    if (lights[i].side == activeSide) {
      changed = true;
      LightArea& l = lights[i];
      if (--l.turns <= 0) lights.erase(lights.begin() + (long)i);
      else if (l.shrink > 0 && (l.radius -= l.shrink) < 1) lights.erase(lights.begin() + (long)i);  // a torch that burnt down
    }
  if (changed) markSight();
}

void Battle::beginTurn(std::vector<Event>& events) {
  lightsTurnStart();
  for (Unit& u : units)
    if (u.side == activeSide) {
      u.moved = u.done = false;
      // The turn flags of "move again" / "act again" (docs/SRPG.md) start over with them.
      u.moveSpent = 0;
      u.acted = u.refreshed = false;
      u.refreshedBy.clear();  // docs/SRPG.md "Refresh an ally"
    }

  Event e;
  e.type = EventType::TurnStarted;
  e.a = activeSide;
  e.b = round;
  events.push_back(e);

  if (db->rules.economy) {
    std::vector<int> income(db->resources.size(), 0);
    // Once per owned structure, however many squares it covers.
    for (const StructureState& st : structures) {
      if (st.owner != activeSide) continue;
      for (const Amount& a : defOf(st).income) income[a.resource] += a.amount;
    }
    for (size_t r = 0; r < income.size(); ++r) changeResource(activeSide, (int)r, resourceGain(activeSide, income[r]), events);  // docs/DIFFICULTY.md
  }
  researchTurn(events);
  // Repair and supply (docs/ECONOMY.md): after income (it can pay for repairs) and research (a finished bonus
  // changes the maximum HP). Resupply comes before the fuel burns so a unit on a depot is not destroyed by it,
  // and a unit that runs dry is gone before it costs a repair.
  resupplyTurn(events);
  burnFuel(events);
  if (over) return;  // running dry ended the battle: nothing else happens this turn
  repairTurn(events);
  // Strategy-RPG features (docs/SRPG.md): ability waits, summon lifetimes, postponed promotions and "turnStart"
  // skills, after repair and supply and before the story's turn events.
  srpgTurnStart(events);
  if (over) return;  // a condition's damage or a berserk unit's fight ended the battle

  StoryOccurrence o;
  o.kind = Trigger::Kind::Turn;
  o.side = activeSide;
  o.round = round;
  fireStory(o, events);
}

void Battle::advanceTurn(std::vector<Event>& events) {
  if (over) return;
  int count = (int)sides.size();
  for (int i = 0; i < count; ++i) {
    ++activeSide;
    if (activeSide >= count) {
      activeSide = 0;
      ++round;
    }
    if (!sides[activeSide].defeated) {
      beginTurn(events);
      // A unit running dry at the start of this turn can have wiped the side out: it passes at once.
      if (over || !sides[activeSide].defeated) return;
    }
  }
}

// True while the side owns at least one structure tagged "hq".
bool Battle::ownsHq(int side) const {
  for (const StructureState& st : structures) {
    if (st.owner != side) continue;
    for (const std::string& tag : defOf(st).tags)
      if (tag == "hq") return true;
  }
  return false;
}

void Battle::checkDefeat(std::vector<Event>& events) {
  if (over) return;
  const RulesConfig& rules = db->rules;
  std::vector<int> fallen;  // units that left with a defeated side: story events hear of them afterwards

  for (int s = 0; s < (int)sides.size(); ++s) {
    SideState& side = sides[s];
    if (side.defeated) continue;

    bool lost = false;
    if (rules.defeatLeaderDead && side.leaderLost) lost = true;
    if (!lost && rules.defeatNoUnits && side.everHadUnits) {
      bool any = false;
      for (const Unit& u : units)
        if (exists(u) && u.side == s && !isSummoned(u)) { any = true; break; }  // R1: a carried unit counts; A1: a summon does not
      if (!any) lost = true;
    }
    if (!lost && rules.defeatLostHq && side.hadHq && !ownsHq(s)) lost = true;
    if (!lost) continue;

    side.defeated = true;
    Event e;
    e.type = EventType::SideDefeated;
    e.a = s;
    events.push_back(e);
    // A defeated side leaves the field entirely. R1: what it was carrying goes with it (before its carriers, whose
    // fall would otherwise put the carried unit back on the board).
    for (size_t i = 0; i < units.size(); ++i)
      if (units[i].carriedBy >= 0 && units[i].side == s) {
        defeatCarried((int)i, events);
        fallen.push_back((int)i);
      }
    for (size_t i = 0; i < units.size(); ++i)
      if (units[i].alive && units[i].side == s) {
        killUnit((int)i, events);
        fallen.push_back((int)i);
      }
    // Its structures become neutral (and any capture in progress stops). SideDefeated is public, so every team
    // learns that they are neutral now, seen or not.
    markSight();
    for (size_t i = 0; i < structures.size(); ++i) {
      StructureState& st = structures[i];
      if (st.owner != s) continue;
      st.owner = -1;
      st.capture = defOf(st).capturePoints;
      st.capturer = -1;
      learnOwner((int)i, kSeenByAll);
    }
  }

  // Events may add units or end the battle themselves; they run after the sweep above is complete.
  for (int u : fallen) unitDefeated(u, -1, events);

  int survivingTeam = -1;
  bool several = false;
  for (const SideState& side : sides) {
    if (side.defeated) continue;
    if (survivingTeam == -1) survivingTeam = side.team;
    else if (survivingTeam != side.team) several = true;
  }
  if (!several) endBattle(survivingTeam, events);
}

void Battle::unitDefeated(int unit, int killer, std::vector<Event>& events) {
  StoryOccurrence o;
  o.kind = Trigger::Kind::Defeated;
  o.unit = unit;
  o.other = killer;
  fireStory(o, events);
}

}  // namespace ote
