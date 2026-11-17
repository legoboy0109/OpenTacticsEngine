// Story commands that keep a unit in the story while it is off the map, and the roster conditions (docs/EVENTS.md "Taking a unit off
// the map and bringing it back", "Campaign characters in map events").
//
// A hidden unit is represented like a removed one (alive == false, no square: about ninety places skip such a unit before touching its
// cell) plus `Unit::hidden`, and it keeps its hit points, items, level, experience, conditions (they do not tick while it is away) and
// every other number. `exists()` stays false for it, so it counts for no defeat rule and no objective.
#include "core/base/log.h"
#include "core/battle/battle.h"

namespace ote {

namespace {
Event unitEvent(EventType t, int a, int b) {
  Event e;
  e.type = t;
  e.a = a;
  e.b = b;
  return e;
}
}  // namespace

void Battle::storyHideUnit(int i, std::vector<Event>& events) {
  Unit& u = units[(size_t)i];
  const Cell cell = u.cell;
  events.push_back(unitEvent(EventType::UnitRemoved, i, cell));  // the player fades it out, as for a removal
  markSight();
  releaseCapture(i, kNoCell);
  occupant[(size_t)cell] = -1;
  u.alive = false;  // off the map; unlike a removal it keeps its hit points (the unit is still in the story)
  u.hidden = true;
  u.cell = kNoCell;
  srpgUnitGone(i, -1, events);  // its summons vanish, a unit it carried is put down (docs/SRPG.md)
}

bool Battle::storyShowUnit(int i, Cell want, std::vector<Event>& events) {
  Unit& u = units[(size_t)i];
  if (!u.hidden) return false;
  if (sides[(size_t)u.side].defeated) {
    OTE_INFO("story: showUnit '%s': its side is defeated; the unit stays hidden", u.storyId.c_str());
    return false;
  }
  const Cell cell = nearestFreeCell(want, typeOf(u).moveType);
  if (cell == kNoCell) {
    OTE_ERROR("story: showUnit '%s': no free square on the map for it", u.storyId.c_str());
    return false;
  }
  markSight();
  u.hidden = false;
  u.alive = true;
  u.cell = cell;
  occupant[(size_t)cell] = i;
  sides[(size_t)u.side].everHadUnits = true;
  events.push_back(unitEvent(EventType::UnitAdded, i, cell));
  // It has stopped on a square: events waiting for a unit to arrive there may run (as after a `moveUnit`).
  StoryOccurrence o;
  o.kind = Trigger::Kind::Reach;
  o.unit = i;
  o.cell = cell;
  fireStory(o, events);
  return true;
}

// rosterAlive / rosterDead / inRoster. The map's own unit wins where there is one (rosterAlive, rosterDead): a character on this map
// is alive when it stands on it or is hidden, dead when it was defeated or removed. A character that is not on this map is read from the
// roster as it was when the chapter began: the active party's, or with `party: any` every party's. Without a roster (a free battle, or
// a game with no campaign) only the map speaks; `inRoster` then asks whether the unit is on the map or hidden.
bool Battle::storyRosterHolds(const Condition& c) const {
  const std::string& id = c.filter.unit;
  const int on = unitByStoryId(id);
  const bool mapAlive = on >= 0 && (exists(units[(size_t)on]) || units[(size_t)on].hidden);
  bool inAlive = false, inGone = false;
  const auto look = [&](const Roster& list) {
    for (const RosterUnit& r : list)
      if (r.id == id) (r.gone ? inGone : inAlive) = true;
  };
  if (roster) look(*roster);
  if (c.anyParty && otherRosters)
    for (const Roster& r : *otherRosters) look(r);
  switch (c.rosterMode) {
    case 0: return on >= 0 ? mapAlive : inAlive;
    case 1: return on >= 0 ? !mapAlive : (inGone && !inAlive);
    default: return roster ? inAlive : mapAlive;
  }
}

}  // namespace ote
