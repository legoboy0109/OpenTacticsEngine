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

// The number a setVariable / addVariable reads from a unit (docs/EVENTS.md "Reading a unit's numbers"). A unit that is
// not on the map yet, was removed or was defeated reads 0; a carried (rescued) unit still exists and is read.
int readUnitNumber(const Battle& b, const UnitReading& r) {
  const int i = b.unitByStoryId(r.unit);
  if (i < 0) return 0;
  const Unit& u = b.units[(size_t)i];
  if (!b.exists(u)) return 0;
  switch (r.kind) {
    case UnitReading::Kind::Level: return u.level;  // the level shown on the unit (a promotion may have reset it)
    case UnitReading::Kind::Hp: return std::max(0, u.hp);
    case UnitReading::Kind::MaxHp: return b.maxHp(u);
    case UnitReading::Kind::Stat: return b.stat(u, r.stat);  // as the rules see it, accessories and research included
    case UnitReading::Kind::None: break;
  }
  return 0;
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
  u.id = (int)units.size();
  u.cell = cell;
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

void Battle::runCommands(const std::vector<Command>& list, std::vector<Event>& events) {
  for (const Command& c : list) {
    if (choice_.halted) return;  // a question is waiting behind us (story_choice.cpp): nothing after it counts
    runCommand(c, events);
  }
}

void Battle::runCommand(const Command& c, std::vector<Event>& events) {
  using K = Command::Kind;
  const int unitCount = (int)units.size();  // units added by this command are not its targets
  switch (c.kind) {
    case K::Say: {
      Event e = makeEvent(EventType::Dialogue, c.unit.empty() ? -1 : unitByStoryId(c.unit), c.text, c.speakerName, c.portrait);
      // The presentation keys ride along: position and narrate in the flags, "auto" in data (docs/EVENTS.md "Scene presentation").
      e.flags = (c.fx.position & kDialoguePosMask) | (c.fx.narrate ? kDialogueNarrate : 0);
      if (c.fx.ms > 0) e.data.push_back(c.fx.ms);
      events.push_back(e);
      break;
    }
    case K::Choice:
      storyChoice(c, events);
      break;
    case K::AddUnit:
      storyAddUnit(c.newUnit, events);
      break;
    case K::ShowUnit: {
      const int u = unitByStoryId(c.unit);
      if (u >= 0 && units[(size_t)u].hidden) storyShowUnit(u, grid.at(c.x, c.y), events);
      else OTE_INFO("story: showUnit '%s': no such hidden unit", c.unit.c_str());
      break;
    }
    case K::RemoveUnit:
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
      break;
    case K::MoveUnit: {
      int u = unitByStoryId(c.unit);
      if (u >= 0 && units[(size_t)u].alive) storyRelocate(u, grid.at(c.x, c.y), events, c.fx.walk, c.fx.wait == 0);
      else if (u >= 0 && units[(size_t)u].carriedBy >= 0) storyMoveCarried(u, grid.at(c.x, c.y), events);  // R1: dropped there
      break;
    }
    case K::ChangeSide:
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
    case K::TakeItem:
      // A unit that exists (on the board, or carried: R1) loses one copy, the first in its inventory, or all of them.
      // The equipped weapon is simply the first wieldable item left, so it falls back by itself (docs/ITEMS.md).
      for (int i = 0; i < unitCount; ++i) {
        if (!exists(units[(size_t)i]) || !unitMatches(c.filter, units[(size_t)i])) continue;
        std::vector<ItemStack>& items = units[(size_t)i].items;
        for (size_t s = 0; s < items.size();) {
          if (items[s].item != c.item) { ++s; continue; }
          items.erase(items.begin() + (long)s);
          if (!c.on) break;
        }
      }
      break;
    case K::GiveResource:
      for (const Amount& a : c.amounts) {
        // Funds never go below zero (nor past the save limit).
        long long now = sides[(size_t)c.side].resources[(size_t)a.resource];
        long long to = std::max(0LL, std::min((long long)kMaxStoryNumber, now + resourceGain(c.side, a.amount)));  // docs/DIFFICULTY.md: a gain is scaled for a human side
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
      storyState.variables[(size_t)c.index] =
          c.reading.kind == UnitReading::Kind::None ? c.value : clampStory(readUnitNumber(*this, c.reading));
      break;
    case K::AddVariable:
      storyState.variables[(size_t)c.index] = clampStory((long long)storyState.variables[(size_t)c.index] +
                                                                (c.reading.kind == UnitReading::Kind::None ? c.value : readUnitNumber(*this, c.reading)));
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
      if (storyRun_.talker >= 0 && units[(size_t)storyRun_.talker].alive) {
        units[(size_t)storyRun_.talker].moved = true;
        endOrder(storyRun_.talker);  // like every action that ends a unit's turn
      }
      break;
    case K::PlayMusic:
      storyState.musicAsked = true;
      storyState.music = storyText(c.text);
      events.push_back(makeEvent(EventType::MusicChanged, c.text, c.value));
      break;
    case K::StopMusic:
      storyState.musicAsked = true;
      storyState.music.clear();
      events.push_back(makeEvent(EventType::MusicChanged, -1, c.value));
      break;
    case K::PlaySound:
      events.push_back(makeEvent(EventType::SoundRequested, c.text));
      break;
    // Strategy-RPG features (docs/SRPG.md): each area runs its own commands.
    case K::Promote:
      storyPromote(c, events);
      break;
    case K::TeachSkill:
      storyTeachSkill(c, events);
      break;
    case K::ForgetSkill:
      storyForgetSkill(c, events);
      break;
    case K::AddSupport:
      storyAddSupport(c, events);
      break;
    case K::Place:  // doors and chests (places_apply.cpp)
      storyPlace(c, events);
      break;
    case K::AddCondition:
      storyAddCondition(c, events);
      break;
    case K::RemoveCondition:
      storyRemoveCondition(c, events);
      break;
    case K::SetBehaviour:  // docs/AI.md
      for (int i = 0; i < unitCount; ++i) {
        Unit& u = units[(size_t)i];
        if (!u.alive || !unitMatches(c.filter, u) || !sides[(size_t)u.side].ai) continue;
        u.behaviour = c.behaviour;
        u.awake = false;  // a guard starts waiting afresh
      }
      break;
    case K::WakeGroup:
      wakeGroup(c.unit);
      break;
    // Changing the map and fog (mapchange.cpp).
    case K::SetTerrain: {
      std::vector<Cell> squares;
      for (const Rect& r : c.rects)
        for (int y = r.y; y < r.y + r.h; ++y)
          for (int x = r.x; x < r.x + r.w; ++x)
            if (grid.at(x, y) != kNoCell) squares.push_back(grid.at(x, y));
      storySetTerrain(squares, c.index, events, c.value, c.on);  // value = the tile (kTileUnset = none given), on = keepTile
      break;
    }
    case K::ApplyChange: storyApplyChange(c.index, events); break;
    case K::RevertChange: storyRevertChange(c.index, events); break;
    case K::SetFog: storySetFog(c.on); break;
    case K::SetVision: storySetVision(c.value); break;
    case K::StoreItems: storyStoreItems(c, events); break;  // story_extras.cpp
    case K::Fight: storyFight(c.fight, events); break;      // story_fight.cpp
    case K::Focus: {
      int u = c.unit.empty() ? -1 : unitByStoryId(c.unit);
      Event f;
      if (!c.unit.empty()) {
        if (u < 0 || !units[(size_t)u].alive) break;
        f = makeEvent(EventType::Focus, units[(size_t)u].cell, u);
      } else {
        f = makeEvent(EventType::Focus, grid.at(c.x, c.y), -1);
      }
      f.c = c.fx.ms;  // scroll milliseconds (-1 = not given: the player's usual half second); "wait": false in the flags
      if (c.fx.speedMilli > 0) f.data.push_back(c.fx.speedMilli);  // "speed": the player works the time out from the distance
      f.flags = c.fx.wait == 0 ? kFocusNoWait : 0;
      events.push_back(f);
      break;
    }
    case K::Scene:  // docs/EVENTS.md "Scene presentation" (story_scene_fx.cpp)
      storyScene(c, events);
      break;
    case K::SetPhaseMusic: storyPhaseMusic(c, events); break;  // story_music.cpp
  }
}

}  // namespace ote
