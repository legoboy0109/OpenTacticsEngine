// The event commands that put units on the map, move them, take them off or change them
// (docs/EVENTS.md "Commands"). story_commands.cpp dispatches to these.
//
// Careful: addUnit grows `units`, so no reference into it is kept across a call that may add one.
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
  if (!appearsOnDifficulty(def)) return;  // limited to other difficulties (docs/DIFFICULTY.md)
  Unit u = makeUnit(def);
  {  // the id of a hidden unit: it comes back (the same unit, with its state) at the square the addUnit names
    const int hid = unitByStoryId(u.storyId);
    if (hid >= 0 && units[(size_t)hid].hidden) {
      storyShowUnit(hid, grid.at(def.x, def.y), events);
      return;
    }
  }
  if (u.character >= 0 && unitByStoryId(u.storyId) >= 0) {
    // A character is one person: adding one that is already in the battle (alive or not) adds nothing.
    OTE_INFO("story: addUnit: character '%s' is already in the battle; not added", u.storyId.c_str());
    return;
  }
  if (!u.storyId.empty() && unitByStoryId(u.storyId) >= 0) {
    // Ids stay unique: a repeating event adds a second one without the id.
    OTE_WARN("story: addUnit: a unit with id '%s' already exists; the new one gets no id", u.storyId.c_str());
    u.storyId.clear();
    // Without the id it is nobody in particular (docs/SRPG.md "Characters"): dressed again as a plain unit.
    u.learned.clear();
    dressUnit(u);
    u.hp = maxHp(u);
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
  markSight();
  if (u.behaviour.idleTurns > 0 && u.side == activeSide) u.idleSeen = 1;  // docs/AI.md: arriving in its own side's turn, that turn counts as its first
  u.id = (int)units.size();
  u.cell = cell;
  if (u.facing < 0) u.facing = defaultFacing(cell, teamOf(u));  // rules.facing: the story's own "facing", else the nearest foe (-1 without facing)
  occupant[(size_t)cell] = u.id;
  sides[(size_t)u.side].everHadUnits = true;
  units.push_back(u);
  events.push_back(makeEvent(EventType::UnitAdded, u.id, cell));
}

void Battle::storyRelocate(int unit, Cell to, std::vector<Event>& events, bool walk, bool noWait) {
  Cell from = units[(size_t)unit].cell;
  occupant[(size_t)from] = -1;  // its own square counts as free for the search
  Cell dest = nearestFreeCell(to, typeOf(units[(size_t)unit]).moveType);
  occupant[(size_t)from] = unit;
  if (dest == kNoCell || dest == from) return;
  releaseCapture(unit, dest);
  markSight();
  occupant[(size_t)from] = -1;
  occupant[(size_t)dest] = unit;
  units[(size_t)unit].cell = dest;
  Event moved = makeEvent(EventType::UnitRelocated, unit, from, dest);
  if (walk) {  // docs/EVENTS.md "moveUnit": the same relocation, played as a walk along the way (no way found: the usual slide)
    if (storyWalkPath(unit, from, dest, moved.data)) moved.flags = kRelocateWalk | (noWait ? kFocusNoWait : 0);
  }
  events.push_back(moved);
  // The unit has stopped somewhere: events waiting for that unit to arrive there may run.
  StoryOccurrence o;
  o.kind = Trigger::Kind::Reach;
  o.unit = unit;
  o.cell = dest;
  fireStory(o, events);
}

void Battle::cmdShowUnit(const Command& c, std::vector<Event>& events) {
  const int u = unitByStoryId(c.unit);
  if (u >= 0 && units[(size_t)u].hidden) storyShowUnit(u, grid.at(c.x, c.y), events);
  else OTE_INFO("story: showUnit '%s': no such hidden unit", c.unit.c_str());
}

void Battle::cmdRemoveUnit(const Command& c, std::vector<Event>& events) {
  const int unitCount = (int)units.size();  // units added by this command are not its targets
  for (int i = 0; i < unitCount; ++i) {
    if (units[(size_t)i].hidden && !c.hide && unitMatches(c.filter, units[(size_t)i])) {  // a hidden unit that is removed for good is gone now
      units[(size_t)i].hidden = false;
      units[(size_t)i].hp = 0;
      units[(size_t)i].conditions.clear();
      continue;
    }
    if (!exists(units[(size_t)i]) || !unitMatches(c.filter, units[(size_t)i])) continue;  // R1: a carried unit matches
    if (c.hide) {  // docs/EVENTS.md "Taking a unit off the map and bringing it back"
      if (units[(size_t)i].carriedBy >= 0) OTE_INFO("story: removeUnit hide: '%s' is carried; it cannot be hidden", units[(size_t)i].storyId.c_str());
      else storyHideUnit(i, events);
      continue;
    }
    if (units[(size_t)i].carriedBy >= 0) {  // taken out of its carrier (docs/SRPG.md "Carry": Story)
      storyRemoveCarried(i, events);
      continue;
    }
    Cell cell = units[(size_t)i].cell;
    events.push_back(makeEvent(EventType::UnitRemoved, i, cell));
    markSight();
    releaseCapture(i, kNoCell);
    occupant[(size_t)cell] = -1;
    units[(size_t)i].alive = false;  // gone, but not "defeated": no drops, no defeat events
    units[(size_t)i].hp = 0;
    units[(size_t)i].cell = kNoCell;
    srpgUnitGone(i, -1, events);  // its summons vanish, a unit it carried is put down (docs/SRPG.md)
  }
}

void Battle::cmdMoveUnit(const Command& c, std::vector<Event>& events) {
  int u = unitByStoryId(c.unit);
  if (u >= 0 && units[(size_t)u].alive) storyRelocate(u, grid.at(c.x, c.y), events, c.fx.walk, c.fx.wait == 0);
  else if (u >= 0 && units[(size_t)u].carriedBy >= 0) storyMoveCarried(u, grid.at(c.x, c.y), events);  // R1: dropped there
}

void Battle::cmdChangeSide(const Command& c, std::vector<Event>& events) {
  const int unitCount = (int)units.size();
  for (int i = 0; i < unitCount; ++i) {
    if (!exists(units[(size_t)i]) || units[(size_t)i].side == c.side || !unitMatches(c.filter, units[(size_t)i])) continue;
    // R1: a carrier, or a unit that is carried, first puts the carried unit down; if there is no square for it
    // the command leaves both units as they are.
    if (!storySeparate(i, events)) continue;
    Unit& u = units[(size_t)i];
    events.push_back(makeEvent(EventType::SideChanged, i, u.side, c.side));
    markSight();
    releaseCapture(i, kNoCell);  // a capture belongs to its capturer's side
    u.side = c.side;
    sides[(size_t)c.side].everHadUnits = true;
    clampHp(u);  // the new side's research may give it another maximum
  }
}

void Battle::cmdSetBehaviour(const Command& c) {  // docs/AI.md
  const int unitCount = (int)units.size();
  for (int i = 0; i < unitCount; ++i) {
    Unit& u = units[(size_t)i];
    if (!u.alive || !unitMatches(c.filter, u) || !sides[(size_t)u.side].ai) continue;
    u.behaviour = c.behaviour;
    u.awake = false;  // a guard starts waiting afresh
    u.retreating = false, u.patrolAt = 0;  // the scripted-style state (docs/AI.md) starts afresh too
    u.idleSeen = u.behaviour.idleTurns > 0 && u.side == activeSide ? 1 : 0;  // the turn it is handed out in counts as the first idle one
  }
}

}  // namespace ote
