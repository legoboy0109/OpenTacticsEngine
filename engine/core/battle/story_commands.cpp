// What each event command does (docs/EVENTS.md "Commands"). Commands change the battle at once
// and append the entries the player shows in order.
//
// Careful: addUnit grows `units`, so no reference into it is kept across a call that may add one.
#include <algorithm>

#include "core/base/log.h"
#include "core/battle/battle.h"

namespace ote {

namespace {

Event makeEvent(EventType t, int a = -1, int b = -1, int c = -1, int d = -1) {
  Event e;
  e.type = t;
  e.a = a;
  e.b = b;
  e.c = c;
  e.d = d;
  return e;
}

int clampStory(long long v) {
  const long long lim = kMaxStoryNumber;
  return (int)std::max(-lim, std::min(lim, v));
}

}  // namespace

// The free square nearest to `target` (by grid distance, lowest cell index on a tie) that a unit
// of this move type can stand on; kNoCell if the map has none left.
Cell Battle::nearestFreeCell(Cell target, int moveType) const {
  Cell best = kNoCell;
  int bestDist = 0;
  for (Cell c = 0; c < grid.cellCount(); ++c) {
    if (occupant[(size_t)c] != -1 || enterCost(c, moveType) <= 0) continue;
    int d = grid.distance(target, c);
    if (best == kNoCell || d < bestDist) {
      best = c;
      bestDist = d;
    }
  }
  return best;
}

void Battle::storyAddUnit(const MapUnitDef& def, std::vector<Event>& events) {
  Unit u = makeUnit(def);
  if (!u.storyId.empty() && unitByStoryId(u.storyId) >= 0) {
    // Ids stay unique: a repeating event adds a second one without the id.
    OTE_WARN("story: addUnit: a unit with id '%s' already exists; the new one gets no id", u.storyId.c_str());
    u.storyId.clear();
  }
  if (applyRoster(u) == 0) {  // a campaign's unit that fell for good does not come back
    OTE_INFO("story: addUnit '%s': the campaign roster says it is gone; not added", u.storyId.c_str());
    return;
  }
  // After the roster: it may change the unit's type, and with it where it can stand.
  Cell cell = nearestFreeCell(grid.at(def.x, def.y), typeOf(u).moveType);
  if (cell == kNoCell) {
    OTE_ERROR("story: addUnit '%s': no free square on the map for it", typeOf(u).id.c_str());
    return;
  }
  u.id = (int)units.size();
  u.cell = cell;
  occupant[(size_t)cell] = u.id;
  sides[(size_t)u.side].everHadUnits = true;
  units.push_back(u);
  events.push_back(makeEvent(EventType::UnitAdded, u.id, cell));
}

void Battle::storyRelocate(int unit, Cell to, std::vector<Event>& events) {
  Cell from = units[(size_t)unit].cell;
  occupant[(size_t)from] = -1;  // its own square counts as free for the search
  Cell dest = nearestFreeCell(to, typeOf(units[(size_t)unit]).moveType);
  occupant[(size_t)from] = unit;
  if (dest == kNoCell || dest == from) return;
  releaseCapture(unit, dest);
  occupant[(size_t)from] = -1;
  occupant[(size_t)dest] = unit;
  units[(size_t)unit].cell = dest;
  events.push_back(makeEvent(EventType::UnitRelocated, unit, from, dest));
  // The unit has stopped somewhere: events waiting for that unit to arrive there may run.
  StoryOccurrence o;
  o.kind = Trigger::Kind::Reach;
  o.unit = unit;
  o.cell = dest;
  fireStory(o, events);
}

void Battle::runCommands(const std::vector<Command>& list, std::vector<Event>& events) {
  for (const Command& c : list) runCommand(c, events);
}

void Battle::runCommand(const Command& c, std::vector<Event>& events) {
  using K = Command::Kind;
  const int unitCount = (int)units.size();  // units added by this command are not its targets
  switch (c.kind) {
    case K::Say: {
      Event e = makeEvent(EventType::Dialogue, c.unit.empty() ? -1 : unitByStoryId(c.unit), c.text, c.speakerName, c.portrait);
      events.push_back(e);
      break;
    }
    case K::AddUnit:
      storyAddUnit(c.newUnit, events);
      break;
    case K::RemoveUnit:
      for (int i = 0; i < unitCount; ++i) {
        if (!units[(size_t)i].alive || !unitMatches(c.filter, units[(size_t)i])) continue;
        Cell cell = units[(size_t)i].cell;
        events.push_back(makeEvent(EventType::UnitRemoved, i, cell));
        releaseCapture(i, kNoCell);
        occupant[(size_t)cell] = -1;
        units[(size_t)i].alive = false;  // gone, but not "defeated": no drops, no defeat events
        units[(size_t)i].hp = 0;
        units[(size_t)i].cell = kNoCell;
      }
      break;
    case K::MoveUnit: {
      int u = unitByStoryId(c.unit);
      if (u >= 0 && units[(size_t)u].alive) storyRelocate(u, grid.at(c.x, c.y), events);
      break;
    }
    case K::ChangeSide:
      for (int i = 0; i < unitCount; ++i) {
        Unit& u = units[(size_t)i];
        if (!u.alive || u.side == c.side || !unitMatches(c.filter, u)) continue;
        events.push_back(makeEvent(EventType::SideChanged, i, u.side, c.side));
        releaseCapture(i, kNoCell);  // a capture belongs to its capturer's side
        u.side = c.side;
        sides[(size_t)c.side].everHadUnits = true;
        clampHp(u);  // the new side's research may give it another maximum
      }
      break;
    case K::GiveItem:
      for (int i = 0; i < unitCount; ++i) {
        if (!units[(size_t)i].alive || !unitMatches(c.filter, units[(size_t)i])) continue;
        ItemStack s;
        s.item = c.item;
        s.uses = db->items[(size_t)c.item].uses;
        giveTo(i, s, 0, events);
      }
      break;
    case K::GiveResource:
      for (const Amount& a : c.amounts) {
        // Funds never go below zero (nor past the save limit).
        long long now = sides[(size_t)c.side].resources[(size_t)a.resource];
        long long to = std::max(0LL, std::min((long long)kMaxStoryNumber, now + a.amount));
        changeResource(c.side, a.resource, (int)(to - now), events);
      }
      break;
    case K::GrantResearch:
      finishResearch(c.side, c.index, events);  // no cost; nothing happens if it is finished already
      break;
    case K::SetSwitch:
      storyState.switches[(size_t)c.index] = c.on ? 1 : 0;
      break;
    case K::SetVariable:
      storyState.variables[(size_t)c.index] = c.value;
      break;
    case K::AddVariable:
      storyState.variables[(size_t)c.index] = clampStory((long long)storyState.variables[(size_t)c.index] + c.value);
      break;
    case K::Objective:
      storyState.objectives = c.objectives;
      events.push_back(makeEvent(EventType::ObjectiveChanged, story->objectives[(size_t)c.objectives].text, c.objectives));
      break;
    case K::Win:
      endBattle(c.team, events);
      break;
    case K::Lose:
      endBattle(winnerAgainst(c.team), events);
      break;
    case K::If:
      runCommands(conditionsHold(c.cond) ? c.thenDo : c.elseDo, events);
      break;
    case K::EndTurn:
      if (storyRun_.talker >= 0 && units[(size_t)storyRun_.talker].alive)
        units[(size_t)storyRun_.talker].moved = units[(size_t)storyRun_.talker].done = true;
      break;
    case K::Focus: {
      int u = c.unit.empty() ? -1 : unitByStoryId(c.unit);
      if (!c.unit.empty()) {
        if (u >= 0 && units[(size_t)u].alive) events.push_back(makeEvent(EventType::Focus, units[(size_t)u].cell, u));
      } else {
        events.push_back(makeEvent(EventType::Focus, grid.at(c.x, c.y), -1));
      }
      break;
    }
  }
}

}  // namespace ote
