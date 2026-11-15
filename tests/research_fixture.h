// Helpers for the upgrade and research tests (docs/ECONOMY.md): the miniature game of battle_fixture.h
// plus new structure kinds, upgrades and technologies, with a map where blue owns most of the
// interesting structures. Everything is built in code (no files), except where a test says so.
//
//   structures added: lab (research), town, foundry (soldier + tank), keep (tag hq), road (foot cost 1, defense 2)
//   upgrades:  city -> town (300, needs masonry)   factory -> foundry (400)   hq -> keep (500)   mire -> road (100)
//   technologies (index: id  cost turns):
//     0 tools 100 1 (soldiers +1 def)   1 masonry 200 2 (needs tools)   2 armor 300 0 (unlocks tank)
//     3 forge 500 1 (unlocks tank, everybody +1 atk)   4 frailty 50 1 (everybody -3 hp)
//     5 vigor 50 1 (soldiers +2 hp)   6 swiftness 10 0 (soldiers +1 mov)
//   map 6 x 3, blue (side 0, 2000 funds): lab (0,0) city (1,0) factory (2,0) hq (3,0) castle 2x2 (4,0) mire (0,1);
//   soldiers: blue (0,2), red (5,2); red (side 1, 2000 funds) owns a lab at (3,2).
#pragma once
#include "story_fixture.h"

namespace restest {

using namespace ote;

enum { Tools, Masonry, Armor, Forge, Frailty, Vigor, Swiftness };  // db.research indices
enum { Lab = 7, Town, Foundry, Keep, Road };                        // db.structures indices (after the fixture's 7)

inline void addResearch(fixture::World& w) {
  Database& db = w.db;
  auto kind = [&](const char* id) {
    StructureDef s;
    s.id = s.name = id;
    s.moveCost = {-1};
    return s;
  };
  StructureDef lab = kind("lab"); lab.research = true; lab.capturePoints = 20;
  StructureDef town = kind("town"); town.capturePoints = 30; town.income = {{0, 200}}; town.produces = {0};
  StructureDef foundry = kind("foundry"); foundry.produces = {0, 2};
  StructureDef keep = kind("keep"); keep.capturePoints = 40; keep.tags = {"hq"};
  StructureDef road = kind("road"); road.moveCost = {1}; road.props = {{"defense", 2}};
  db.structures.push_back(lab);
  db.structures.push_back(town);
  db.structures.push_back(foundry);
  db.structures.push_back(keep);
  db.structures.push_back(road);
  auto upgrade = [&](const char* from, int to, int cost, int needs = -1) {
    UpgradeDef u;
    u.to = to;
    u.cost = {{0, cost}};
    u.needs = needs;
    db.structures[(size_t)db.findStructure(from)].upgrades.push_back(u);
  };
  upgrade("city", Town, 300, Masonry);
  upgrade("factory", Foundry, 400);
  upgrade("hq", Keep, 500);
  upgrade("mire", Road, 100);

  auto tech = [&](const char* id, int cost, int turns) {
    TechnologyDef t;
    t.id = id;
    t.name = id;
    t.name[0] = (char)(t.name[0] - 'a' + 'A');  // "Tools": menu names differ from ids
    t.cost = {{0, cost}};
    t.turns = turns;
    return t;
  };
  TechnologyDef tools = tech("tools", 100, 1); tools.bonusTypes = {0}; tools.bonus = {0, 0, 1, 0};
  TechnologyDef masonry = tech("masonry", 200, 2); masonry.needs = {Tools};
  TechnologyDef armor = tech("armor", 300, 0); armor.unlocksUnits = {2};
  TechnologyDef forge = tech("forge", 500, 1); forge.unlocksUnits = {2}; forge.bonus = {0, 1, 0, 0};
  TechnologyDef frailty = tech("frailty", 50, 1); frailty.bonus = {-3, 0, 0, 0};
  TechnologyDef vigor = tech("vigor", 50, 1); vigor.bonusTypes = {0}; vigor.bonus = {2, 0, 0, 0};
  TechnologyDef swiftness = tech("swiftness", 10, 0); swiftness.bonusTypes = {0}; swiftness.bonus = {0, 0, 0, 1};
  db.research = {tools, masonry, armor, forge, frailty, vigor, swiftness};
}

// A story world (so events can be loaded) with the research map; not started: call begin() / play().
struct ResearchWorld : story::StoryWorld {
  ResearchWorld()
      : StoryWorld({"......", "......", "......"}, {{"soldier", 0, 0, 2, false}, {"soldier", 1, 5, 2, false}}, {"hero", "foe"}) {
    addResearch(*this);
    db.rules.economy = db.rules.production = db.rules.capture = true;
    place("lab", 0, 0, 0);
    place("city", 1, 0, 0);
    place("factory", 2, 0, 0);
    place("hq", 3, 0, 0);
    place("castle", 4, 0, 0);
    place("mire", 0, 1, -1);
    place("lab", 3, 2, 1);
    map.sides[0].resources = {{0, 2000}};
    map.sides[1].resources = {{0, 2000}};
  }
  int funds(int side = 0) const { return battle.sides[(size_t)side].resources[0]; }
  int structure(int x, int y) const { return battle.structureAt[(size_t)at(x, y)]; }
  int kindAt(int x, int y) const { return battle.structures[(size_t)structure(x, y)].type; }
  // Both sides end their turns: the blue side's next turn has begun.
  bool nextTurn() { return act(Action::endTurn()) && act(Action::endTurn()); }
  // Runs one action and returns why it was refused ("" = it worked).
  std::string refused(const Action& a) {
    std::string why;
    if (battle.validate(a, &why)) return "";
    return why.empty() ? "refused" : why;
  }
};

inline bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

}  // namespace restest
