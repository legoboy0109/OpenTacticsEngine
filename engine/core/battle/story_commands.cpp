// What each event command does (docs/EVENTS.md "Commands"). Commands change the battle at once
// and append the entries the player shows in order. This file holds the dispatcher and the commands
// on items, resources, variables, the story's flow and the camera; the unit commands are in
// story_command_units.cpp.
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

void Battle::runCommands(const std::vector<Command>& list, std::vector<Event>& events) {
  for (const Command& c : list) {
    if (choice_.halted) return;  // a question is waiting behind us (story_choice.cpp): nothing after it counts
    runCommand(c, events);
  }
}

void Battle::cmdSay(const Command& c, std::vector<Event>& events) {
  Event e = makeEvent(EventType::Dialogue, c.unit.empty() ? -1 : unitByStoryId(c.unit), c.text, c.speakerName, c.portrait);
  // The presentation keys ride along: position and narrate in the flags, "auto" in data (docs/EVENTS.md "Scene presentation").
  e.flags = (c.fx.position & kDialoguePosMask) | (c.fx.narrate ? kDialogueNarrate : 0);
  if (c.fx.ms > 0) e.data.push_back(c.fx.ms);
  events.push_back(e);
}

void Battle::cmdGiveItem(const Command& c, std::vector<Event>& events) {
  const int unitCount = (int)units.size();  // units added by this command are not its targets
  for (int i = 0; i < unitCount; ++i) {
    if (!units[(size_t)i].alive || !unitMatches(c.filter, units[(size_t)i])) continue;
    ItemStack s;
    s.item = c.item;
    s.uses = db->items[(size_t)c.item].uses;
    giveTo(i, s, 0, events);
  }
}

void Battle::cmdTakeItem(const Command& c) {
  // A unit that exists (on the board, or carried: R1) loses one copy, the first in its inventory, or all of them.
  // The equipped weapon is simply the first wieldable item left, so it falls back by itself (docs/ITEMS.md).
  const int unitCount = (int)units.size();
  for (int i = 0; i < unitCount; ++i) {
    if (!exists(units[(size_t)i]) || !unitMatches(c.filter, units[(size_t)i])) continue;
    std::vector<ItemStack>& items = units[(size_t)i].items;
    for (size_t s = 0; s < items.size();) {
      if (items[s].item != c.item) { ++s; continue; }
      items.erase(items.begin() + (long)s);
      if (!c.on) break;
    }
  }
}

void Battle::cmdGiveResource(const Command& c, std::vector<Event>& events) {
  for (const Amount& a : c.amounts) {
    // Funds never go below zero (nor past the save limit).
    long long now = sides[(size_t)c.side].resources[(size_t)a.resource];
    long long to = std::max(0LL, std::min((long long)kMaxStoryNumber, now + resourceGain(c.side, a.amount)));  // docs/DIFFICULTY.md: a gain is scaled for a human side
    changeResource(c.side, a.resource, (int)(to - now), events);
  }
}

void Battle::cmdEndTurn() {
  if (storyRun_.talker >= 0 && units[(size_t)storyRun_.talker].alive) {
    units[(size_t)storyRun_.talker].moved = true;
    endOrder(storyRun_.talker);  // like every action that ends a unit's turn
  }
}

void Battle::cmdSetTerrain(const Command& c, std::vector<Event>& events) {
  std::vector<Cell> squares;
  for (const Rect& r : c.rects)
    for (int y = r.y; y < r.y + r.h; ++y)
      for (int x = r.x; x < r.x + r.w; ++x)
        if (grid.at(x, y) != kNoCell) squares.push_back(grid.at(x, y));
  storySetTerrain(squares, c.index, events, c.value, c.on);  // value = the tile (kTileUnset = none given), on = keepTile
}

void Battle::cmdFocus(const Command& c, std::vector<Event>& events) {
  int u = c.unit.empty() ? -1 : unitByStoryId(c.unit);
  Event f;
  if (!c.unit.empty()) {
    if (u < 0 || !units[(size_t)u].alive) return;
    f = makeEvent(EventType::Focus, units[(size_t)u].cell, u);
  } else {
    f = makeEvent(EventType::Focus, grid.at(c.x, c.y), -1);
  }
  f.c = c.fx.ms;  // scroll milliseconds (-1 = not given: the player's usual half second); "wait": false in the flags
  if (c.fx.speedMilli > 0) f.data.push_back(c.fx.speedMilli);  // "speed": the player works the time out from the distance
  f.flags = c.fx.wait == 0 ? kFocusNoWait : 0;
  events.push_back(f);
}

void Battle::runCommand(const Command& c, std::vector<Event>& events) {
  using K = Command::Kind;
  // docs/EVENTS.md "Showing another map" (story_show_map.cpp): while a map is shown, the commands on units, squares and the camera act on it.
  if (shown_ && onShownMap(c)) {
    shown_->runCommand(c, events);
    return;
  }
  switch (c.kind) {
    case K::ShowMap: storyShowMap(c, events); break;
    case K::ReturnMap: storyReturnMap(events); break;
    case K::Say: cmdSay(c, events); break;
    case K::Choice: storyChoice(c, events); break;
    case K::AddUnit: storyAddUnit(c.newUnit, events); break;
    case K::ShowUnit: cmdShowUnit(c, events); break;
    case K::RemoveUnit: cmdRemoveUnit(c, events); break;
    case K::MoveUnit: cmdMoveUnit(c, events); break;
    case K::ChangeSide: cmdChangeSide(c, events); break;
    case K::GuestJoins: cmdGuestJoins(c); break;
    case K::GiveItem: cmdGiveItem(c, events); break;
    case K::TakeItem: cmdTakeItem(c); break;
    case K::GiveResource: cmdGiveResource(c, events); break;
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
    case K::Win: endBattle(c.team, events); break;
    case K::Lose: endBattle(winnerAgainst(c.team), events); break;
    case K::If: runCommands(conditionsHold(c.cond) ? c.thenDo : c.elseDo, events); break;
    case K::EndTurn: cmdEndTurn(); break;
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
    case K::PlaySound: events.push_back(makeEvent(EventType::SoundRequested, c.text)); break;
    // Strategy-RPG features (docs/SRPG.md): each area runs its own commands.
    case K::Promote: storyPromote(c, events); break;
    case K::TeachSkill: storyTeachSkill(c, events); break;
    case K::ForgetSkill: storyForgetSkill(c, events); break;
    case K::AddSupport: storyAddSupport(c, events); break;
    case K::Place: storyPlace(c, events); break;  // doors and chests (places_apply.cpp)
    case K::AddCondition: storyAddCondition(c, events); break;
    case K::RemoveCondition: storyRemoveCondition(c, events); break;
    case K::SetBehaviour: cmdSetBehaviour(c); break;
    case K::WakeGroup: wakeGroup(c.unit); break;
    // Changing the map and fog (mapchange.cpp).
    case K::SetTerrain: cmdSetTerrain(c, events); break;
    case K::ApplyChange: storyApplyChange(c.index, events); break;
    case K::RevertChange: storyRevertChange(c.index, events); break;
    case K::SetFog: storySetFog(c.on); break;
    case K::SetVision: storySetVision(c.value); break;
    case K::StoreItems: storyStoreItems(c, events); break;  // story_extras.cpp
    case K::Fight: storyFight(c.fight, events); break;      // story_fight.cpp
    case K::Focus: cmdFocus(c, events); break;
    case K::Scene: storyScene(c, events); break;  // docs/EVENTS.md "Scene presentation" (story_scene_fx.cpp)
    case K::SetPhaseMusic: storyPhaseMusic(c, events); break;  // story_music.cpp
  }
}

}  // namespace ote
