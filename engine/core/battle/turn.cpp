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

void Battle::beginTurn(std::vector<Event>& events) {
  for (Unit& u : units)
    if (u.side == activeSide) u.moved = u.done = false;

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
    for (size_t r = 0; r < income.size(); ++r) changeResource(activeSide, (int)r, income[r], events);
  }
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
      return;
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

  for (int s = 0; s < (int)sides.size(); ++s) {
    SideState& side = sides[s];
    if (side.defeated) continue;

    bool lost = false;
    if (rules.defeatLeaderDead && side.leaderLost) lost = true;
    if (!lost && rules.defeatNoUnits && side.everHadUnits) {
      bool any = false;
      for (const Unit& u : units)
        if (u.alive && u.side == s) { any = true; break; }
      if (!any) lost = true;
    }
    if (!lost && rules.defeatLostHq && side.hadHq && !ownsHq(s)) lost = true;
    if (!lost) continue;

    side.defeated = true;
    Event e;
    e.type = EventType::SideDefeated;
    e.a = s;
    events.push_back(e);
    // A defeated side leaves the field entirely.
    for (Unit& u : units)
      if (u.alive && u.side == s) killUnit(u.id, events);
    // Its structures become neutral (and any capture in progress stops).
    for (StructureState& st : structures)
      if (st.owner == s) {
        st.owner = -1;
        st.capture = defOf(st).capturePoints;
        st.capturer = -1;
      }
  }

  int survivingTeam = -1;
  bool several = false;
  for (const SideState& side : sides) {
    if (side.defeated) continue;
    if (survivingTeam == -1) survivingTeam = side.team;
    else if (survivingTeam != side.team) several = true;
  }
  if (!several) {
    over = true;
    winnerTeam = survivingTeam;
    Event e;
    e.type = EventType::BattleEnded;
    e.a = winnerTeam;
    events.push_back(e);
  }
}

}  // namespace ote
