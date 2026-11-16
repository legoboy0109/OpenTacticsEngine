// Starting a chapter's battle from a playthrough and taking its result back (docs/EVENTS.md
// "Campaign" - Carry-over).
#include <memory>

#include "core/campaign/playthrough.h"
#include "core/data/loader.h"
#include "core/script/formulas.h"

namespace ote {

namespace {

// Carried storage first, then what the map put there (the map's own entries were placed by init).
void takeCarried(const Playthrough& pt, Battle& battle) {
  for (SideState& side : battle.sides) {
    if (side.ai) continue;
    for (const CarriedSide& c : pt.sides) {
      if (c.id != side.id) continue;
      for (size_t i = 0; i < side.resources.size() && i < c.resources.size(); ++i) side.resources[i] += c.resources[i];
      if (!c.research.empty()) battle.setStartingResearch((int)(&side - &battle.sides[0]), c.research);
      std::vector<ItemStack> storage;
      for (const ItemGrant& g : c.storage) {
        ItemStack s;
        s.item = g.item;
        s.uses = g.uses;
        s.drop = g.drop;
        storage.push_back(s);
      }
      storage.insert(storage.end(), side.storage.begin(), side.storage.end());
      side.storage = storage;
    }
  }
}

std::vector<ItemGrant> grants(const std::vector<ItemStack>& items) {
  std::vector<ItemGrant> out;
  for (const ItemStack& s : items) {
    ItemGrant g;
    g.item = s.item;
    g.uses = s.uses;
    g.drop = s.drop;
    out.push_back(g);
  }
  return out;
}

RosterUnit rosterEntry(const Battle& b, const Unit& u) {
  RosterUnit r;
  r.id = u.storyId;
  r.type = u.type;
  r.name = u.name;
  r.level = u.level;
  r.xp = u.xp;
  r.stats = u.stats;
  r.items = grants(u.items);
  r.weaponXp = u.weaponXp;  // weapon ranks travel with the unit (docs/SRPG.md section 7)
  r.skills = u.learned;  // skills learned for good travel with the unit (docs/SRPG.md)
  r.side = b.sides[(size_t)u.side].id;
  return r;
}

// Updates or adds the entry for a unit's id.
void putUnit(Roster& roster, const RosterUnit& r) {
  for (RosterUnit& e : roster)
    if (e.id == r.id) {
      e = r;
      return;
    }
  roster.push_back(r);
}

void takeUnits(const Database& db, Playthrough& pt, const Battle& b) {
  for (const Unit& u : b.units) {
    if (u.storyId.empty()) continue;  // extras are never carried
    bool human = !b.sides[(size_t)u.side].ai;
    if (!human) {
      // Recruited away (changeSide to a computer side) or an enemy that was never ours.
      for (size_t i = 0; i < pt.roster.size(); ++i)
        if (pt.roster[i].id == u.storyId) {
          pt.roster.erase(pt.roster.begin() + (long)i);
          break;
        }
      continue;
    }
    if (b.exists(u)) {  // R1: a carried unit goes to the roster as living
      putUnit(pt.roster, rosterEntry(b, u));
    } else if (!db.campaign->fallenReturn) {
      RosterUnit r = rosterEntry(b, u);
      r.gone = true;  // a unit that joined and fell in the same chapter is gone as well
      putUnit(pt.roster, r);
    }
    // "return": a fallen unit stays as the roster had it at the start of the chapter, gaining nothing.
  }
}

}  // namespace

bool prepareChapterBattle(const Database& db, const Playthrough& pt, Formulas& formulas, uint32_t seed, Battle& battle,
                          std::string& err) {
  const ChapterDef* ch = currentChapter(db, pt);
  if (!ch || pt.phase != Phase::Battle) {
    err = "the playthrough is not at a chapter battle";
    return false;
  }
  if (pt.switches.size() != db.switches.size() || pt.variables.size() != db.variables.size()) {
    err = "the playthrough does not match the game's switches and variables";
    return false;
  }
  MapDef map;
  if (!loadMap(db, db.maps[(size_t)ch->map].id, map, err)) return false;
  if (!battle.init(db, map, formulas, seed, err)) return false;
  // Carried technologies first: the roster dresses units at full hit points, bonuses included.
  takeCarried(pt, battle);
  battle.setStartingRoster(std::make_shared<const Roster>(pt.roster));
  battle.setStartingSupports(pt.supports);  // the closeness the campaign carries (docs/SRPG.md); empty = nothing
  if (!battle.setStartingStory(pt.switches, pt.variables)) {
    err = "the playthrough's switches and variables do not fit the game";
    return false;
  }
  return true;
}

bool startChapterBattle(const Database& db, const Playthrough& pt, Formulas& formulas, uint32_t seed, Battle& battle,
                        std::vector<Event>& events, std::string& err) {
  if (!prepareChapterBattle(db, pt, formulas, seed, battle, err)) return false;
  battle.start(events);
  return true;
}

ChapterResult finishChapterBattle(const Database& db, Playthrough& pt, const Battle& battle) {
  if (!db.campaign || pt.phase != Phase::Battle) return ChapterResult::WrongPhase;
  if (!battle.over) return ChapterResult::NotOver;
  if (battle.winnerTeam < 0 || battle.winnerTeam != battle.humanTeam()) {
    if (db.campaign->endOnDefeat) {
      pt.phase = Phase::Over;
      return ChapterResult::Ended;
    }
    return ChapterResult::Retry;
  }
  takeUnits(db, pt, battle);
  for (const SideState& side : battle.sides) {
    if (side.ai) continue;
    CarriedSide& c = pt.sideCarry(side.id, db.resources.size());
    c.resources = side.resources;
    c.storage = grants(side.storage);
    c.research.clear();  // finished ones only; what is still under way is lost with the chapter
    for (size_t t = 0; t < side.researched.size(); ++t)
      if (side.researched[t]) c.research.push_back((int)t);
  }
  pt.switches = battle.storyState.switches;
  pt.variables = battle.storyState.variables;
  takeSupports(db, pt, battle);  // only a won battle changes the closeness (a retry starts from the same)
  pt.phase = Phase::After;
  return ChapterResult::Won;
}

}  // namespace ote
