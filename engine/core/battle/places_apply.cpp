// Doors, chests and visitable places, carrying actions out (docs/SRPG.md section 10 "Rules").
//
// A door, chest or place IS a structure; opening, shutting, visiting and destroying one changes the structure's kind (the engine's
// upgrade mechanism: the footprint stays, movement cost, art, sight and the fog memory follow the kind). The few new numbers of
// state are the lock, a door's hit points, the "visited" mark and a chest's contents.
//
// Order of the events of an opening: ItemBroke (the key's last use), DoorOpened / ChestOpened, then a chest's contents as
// ItemGained / ItemLost / ResourceChanged (flag kLooted), then the story's "place" events, then the unit's order ends.
#include <algorithm>

#include "core/battle/battle.h"
#include "core/data/loader_places.h"

namespace ote {

namespace {

bool refuse(std::string* why, const std::string& reason) {
  if (why) *why = reason;
  return false;
}

Event pev(EventType t, int a = -1, int b = -1, int c = -1, int d = -1, int flags = 0) {
  Event e;
  e.type = t;
  e.a = a;
  e.b = b;
  e.c = c;
  e.d = d;
  e.flags = flags;
  return e;
}

}  // namespace

// ---- validation ----

bool Battle::validatePlace(const Action& a, std::string* why) const {
  const Unit& u = units[(size_t)a.unit];  // validate() has checked the unit
  if (a.structure < 0 || a.structure >= (int)structures.size()) return refuse(why, "there is no such structure");
  const int s = a.structure;
  const StructureState& st = structures[(size_t)s];
  const PlaceDef& p = placeOf(s);
  if (!structureVisibleTo(teamOf(u), s)) return refuse(why, "you cannot see that");
  switch (a.type) {
    case ActionType::OpenPlace: {
      if (!p.isDoor() && !p.isChest()) return refuse(why, "that cannot be opened");
      if (!placeInReach(s, u.cell)) return refuse(why, "it is out of reach");
      if (st.locked) {
        if (!p.byKey) return refuse(why, "keys do not open that");
        if (a.slot < 0 || a.slot >= (int)u.items.size()) return refuse(why, "it is locked and needs a key");
        if (!kindAcceptsKey(*db, defOf(st), st.type, u.items[(size_t)a.slot].item)) return refuse(why, "that key does not fit");
      }
      if (p.isChest() && !roomForLoot(u, s)) return refuse(why, "the unit cannot carry more: drop something first");
      return true;
    }
    case ActionType::VisitPlace:
      if (!p.isVisit() || (st.used && !p.repeat)) return refuse(why, "there is nothing to visit there");
      if (!placeInReach(s, u.cell)) return refuse(why, "it is out of reach");
      if (!visitAllowed(u, s) && !destroyAllowed(u, s)) return refuse(why, "that unit cannot visit it");
      return true;
    case ActionType::BreakPlace: {
      PlaceForecast f;
      if (!doorBreakable(s)) return refuse(why, "that cannot be broken down");
      if (!placeForecast(a.unit, s, a.weapon, u.cell, f)) return refuse(why, "that attack is not possible");
      return true;
    }
    default:
      return refuse(why, "unknown action");
  }
}

// ---- the change itself ----

// Gives a chest's contents out. `unit` >= 0 takes them (its inventory, else the side's storage, else they are lost, as for
// every item); -1 = nobody opened it (a story command): the first human side's storage and funds.
void Battle::giveLoot(int structure, int unit, std::vector<Event>& events) {
  std::vector<PlaceLoot> loot = structures[(size_t)structure].loot;
  structures[(size_t)structure].loot.clear();
  int side = -1;
  if (unit >= 0) side = units[(size_t)unit].side;
  else
    for (size_t i = 0; i < sides.size() && side < 0; ++i)
      if (!sides[i].ai) side = (int)i;
  if (side < 0) side = 0;
  for (const PlaceLoot& l : loot) {
    if (l.item >= 0) {
      ItemStack stack;
      stack.item = l.item;
      stack.uses = l.uses;
      if (unit >= 0) {
        const size_t before = units[(size_t)unit].items.size();
        giveTo(unit, stack, kLooted, events);
        // What a computer unit loots it drops again when it is defeated (the engine's existing "drop" mark).
        Unit& u = units[(size_t)unit];
        if (sides[(size_t)u.side].ai && u.items.size() > before) u.items.back().drop = true;
      } else if (db->rules.items.storage != ItemStorage::Off) {
        sides[(size_t)side].storage.push_back(stack);
        events.push_back(pev(EventType::ItemGained, -1, l.item, side, -1, kLooted));
      } else {
        events.push_back(pev(EventType::ItemLost, -1, l.item));
      }
    } else if (l.resource >= 0) {
      changeResource(side, l.resource, resourceGain(side, l.amount), events);  // docs/DIFFICULTY.md
      if (!events.empty() && events.back().type == EventType::ResourceChanged) events.back().flags |= kLooted;
    }
  }
}

// Opens a closed door or chest: the kind it becomes, the unlock, the event, a chest's contents and the story's "opened" events.
// `unit` = who did it (-1 = a story command or nobody). The caller has judged that it may happen and has no reference into
// `units` across this call (a story event may add units).
void Battle::openPlace(int s, int unit, int how, std::vector<Event>& events) {
  const PlaceDef p = placeOf(s);  // a copy: the kind changes below
  StructureState& st = structures[(size_t)s];
  const int old = st.type;
  markSight();  // events so far describe the world as it was; an opened door changes who sees what
  st.type = p.to;
  st.locked = false;
  st.hp = -1;
  st.capture = defOf(st).capturePoints;
  st.capturer = -1;
  events.push_back(pev(p.isChest() ? EventType::ChestOpened : EventType::DoorOpened, s, unit, how, old));
  if (p.isChest()) giveLoot(s, unit, events);
  StoryOccurrence o;
  o.kind = Trigger::Kind::Place;
  o.place = s;
  o.placeIs = kPlaceOpened;
  o.unit = unit;
  fireStory(o, events);
}

void Battle::visitPlace(int s, int unit, bool destroy, std::vector<Event>& events) {
  const PlaceDef p = placeOf(s);
  StructureState& st = structures[(size_t)s];
  const int old = st.type;
  markSight();
  if (destroy) {
    st.type = p.destroyedTo;
  } else {
    st.used = true;
    if (p.to >= 0) st.type = p.to;
  }
  st.capture = defOf(st).capturePoints;
  st.capturer = -1;
  events.push_back(pev(destroy ? EventType::PlaceDestroyed : EventType::PlaceVisited, s, unit, old));
  StoryOccurrence o;
  o.kind = Trigger::Kind::Place;
  o.place = s;
  o.placeIs = destroy ? kPlaceDestroyed : kPlaceVisited;
  o.unit = unit;
  fireStory(o, events);
}

// ---- actions ----

void Battle::applyPlace(const Action& a, std::vector<Event>& events, bool endsTurn) {
  const int uid = a.unit, s = a.structure;
  switch (a.type) {
    case ActionType::OpenPlace: {
      const bool locked = structures[(size_t)s].locked;
      if (locked) spendUse(uid, a.slot, events);  // before the opening: a key's last use shows first
      openPlace(s, uid, locked ? kOpenKey : kOpenFree, events);
      break;
    }
    case ActionType::VisitPlace:
      visitPlace(s, uid, !visitAllowed(units[(size_t)uid], s), events);
      break;
    case ActionType::BreakPlace: {
      PlaceForecast f;
      placeForecast(uid, s, a.weapon, units[(size_t)uid].cell, f);  // validated
      // Ammunition once per attack, wear of a carried weapon once per strike, like a fight.
      spendAmmo(uid, a.weapon);
      int dealt = 0;
      for (int i = 0; i < f.strikes && structures[(size_t)s].hp > 0; ++i) {
        const int slot = db->rules.items.carriedWeapons ? weaponSlot(units[(size_t)uid], a.weapon) : -1;
        if (db->rules.items.carriedWeapons && slot < 0) break;  // the weapon broke
        StructureState& st = structures[(size_t)s];
        const int damage = std::min(f.damage, st.hp);
        st.hp -= damage;
        dealt += damage;
        events.push_back(pev(EventType::PlaceHit, uid, s, a.weapon, damage, st.hp <= 0 ? kKill : 0));
        if (slot >= 0) spendUse(uid, slot, events);
      }
      if (structures[(size_t)s].hp <= 0) openPlace(s, uid, kOpenBreak, events);
      break;
    }
    default: break;
  }
  if (endsTurn && uid < (int)units.size() && units[(size_t)uid].alive) endOrder(uid);
}

// "rules.places": "actions": "auto" - after a unit ends a move, everything it could open or visit from where it stands happens by
// itself (and does not end its turn). A computer unit only does what it is marked for; nobody destroys by walking past.
void Battle::autoPlaces(int uid, std::vector<Event>& events) {
  for (size_t s = 0; s < structures.size(); ++s) {
    if (over || uid >= (int)units.size() || !units[(size_t)uid].alive) return;
    const Unit& u = units[(size_t)uid];
    const int si = (int)s;
    if (!placeInReach(si, u.cell) || !structureVisibleTo(teamOf(u), si)) continue;
    const bool ai = sides[(size_t)u.side].ai;
    const PlaceDef& p = placeOf(si);
    const StructureState& st = structures[s];
    if (p.isDoor() || p.isChest()) {
      if (ai && !(u.aiGoals & (p.isChest() ? kAiChests : kAiDoors))) continue;
      std::vector<int> keys;
      placeKeySlots(uid, si, keys);
      if (st.locked && keys.empty()) continue;
      const Action act = Action::openPlace(uid, si, st.locked ? keys[0] : -1);
      std::string why;
      if (validatePlace(act, &why)) applyPlace(act, events, false);
    } else if (p.isVisit() && (!st.used || p.repeat)) {
      if (ai && !(u.aiGoals & kAiPlaces)) continue;
      if (!visitAllowed(u, si)) continue;  // destroying is never automatic
      applyPlace(Action::visitPlace(uid, si), events, false);
    }
  }
}

// ---- story: shutting, locking, unlocking ----

// The kind that opens into the structure's present kind (the one the map placed, when it does): what closeDoor goes back to.
int Battle::closedKindOf(int structure) const {
  const StructureState& st = structures[(size_t)structure];
  const StructureDef& placed = db->structures[(size_t)st.mapType];
  if (placed.place.isDoor() && placed.place.to == st.type) return st.mapType;
  for (size_t k = 0; k < db->structures.size(); ++k)
    if (db->structures[k].place.isDoor() && db->structures[k].place.to == st.type) return (int)k;
  return -1;
}

bool Battle::storyShutDoor(int s, bool lock, std::vector<Event>& events) {
  StructureState& st = structures[(size_t)s];
  const bool closed = placeOf(s).isDoor();
  const int target = closed ? st.type : closedKindOf(s);
  if (target < 0 || !db->structures[(size_t)target].place.isDoor()) return false;  // not a door at all
  if (!closed) {
    std::vector<Cell> cells;
    structureCells(s, cells);
    for (Cell c : cells)
      if (occupant[(size_t)c] >= 0) return false;  // somebody stands in the doorway: it stays open
    const int old = st.type;
    markSight();
    st.type = target;
    st.hp = db->structures[(size_t)target].place.hp > 0 ? db->structures[(size_t)target].place.hp : -1;
    st.capture = defOf(st).capturePoints;
    st.capturer = -1;
    st.locked = lock;
    events.push_back(pev(EventType::DoorClosed, s, old, target, -1, lock ? kLocked : 0));
    return true;
  }
  if (lock && !st.locked) {
    st.locked = true;
    events.push_back(pev(EventType::DoorClosed, s, st.type, st.type, -1, kLocked));
  }
  return true;
}

void Battle::storyPlace(const Command& c, std::vector<Event>& events) {
  const int s = placeByStoryId(c.place);
  if (s < 0) return;
  switch (c.index) {
    case kPlaceCmdOpenDoor:
      if (isClosedDoor(s)) openPlace(s, -1, kOpenStory, events);
      break;
    case kPlaceCmdOpenChest: {
      if (!isClosedChest(s)) break;
      const int by = c.unit.empty() ? -1 : unitByStoryId(c.unit);
      openPlace(s, by >= 0 && units[(size_t)by].alive ? by : -1, kOpenStory, events);
      break;
    }
    case kPlaceCmdCloseDoor:
      storyShutDoor(s, false, events);
      break;
    case kPlaceCmdLockDoor:
      storyShutDoor(s, true, events);
      break;
    case kPlaceCmdUnlockDoor:
      if ((isClosedDoor(s) || isClosedChest(s)) && structures[(size_t)s].locked) {
        structures[(size_t)s].locked = false;
        events.push_back(pev(EventType::PlaceUnlocked, s, -1));
      }
      break;
    default: break;
  }
}

}  // namespace ote
