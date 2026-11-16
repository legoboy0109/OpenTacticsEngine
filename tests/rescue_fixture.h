// Helpers for the rescue / pair up tests (docs/SRPG.md section 5; step R1): the miniature game of battle_fixture.h
// with rules.rescue switched on in code.
//   stats (index): hp 0, atk 1, def 2, mov 3.   soldier hp10 atk5 def2 mov3, archer hp8 atk4 def1 mov3,
//   tank hp20 atk8 def5 mov4 (tag "armor").
//   Every test world is 5 x 3 open ground unless it passes rows. Side 0 blue (human) moves first.
#pragma once
#include "story_fixture.h"

namespace rescuetest {

using namespace ote;
using fixture::UnitSpec;

enum { Hp, Atk, Def, Mov };

inline std::vector<std::string> open() { return {".....", ".....", "....."}; }

struct RescueWorld : story::StoryWorld {
  RescueWorld(RescueMode mode, const std::vector<UnitSpec>& units, const std::vector<std::string>& rows = open(),
              const std::vector<const char*>& ids = {})
      : StoryWorld(rows, units, ids) {
    db.rules.rescue.mode = mode;
  }
  // rules.rescue.canCarry (a formula) - before begin().
  void canCarry(const char* text) { db.rules.rescue.canCarry = db.addExpr(text, "test: canCarry"); }
  // rules.rescue.carrierStats: percent first, then the added number (not the health stat).
  void carrierStat(int stat, int percent, int add) {
    if (db.rules.rescue.carrierStats.empty()) db.rules.rescue.carrierStats.assign(db.stats.size(), StatChange());
    db.rules.rescue.carrierStats[(size_t)stat].percent = percent;
    db.rules.rescue.carrierStats[(size_t)stat].add = add;
  }
  // Per stat index, what the unit behind adds (rules.rescue.pairBonus) or, with `type`, what a unit type adds.
  void pairBonus(std::vector<int> bonus, const char* type = nullptr) {
    if (type) db.unitTypes[(size_t)db.findUnitType(type)].pairBonus = bonus;
    else db.rules.rescue.pairBonus = bonus;
  }
  const Unit& un(int id) const { return battle.units[(size_t)id]; }
  int stat(int id, int index) const { return battle.stat(un(id), index); }
  bool refused(const Action& a, const char* why) const {
    std::string got;
    if (battle.validate(a, &got)) return false;
    if (got == why) return true;
    std::printf("  refused with: %s\n  wanted:       %s\n", got.c_str(), why);
    return false;
  }
  // The unit stands on the board at (x, y).
  bool stands(int id, int x, int y) const { return un(id).alive && un(id).cell == battle.grid.at(x, y); }
  // True when the unit is off the board with its hit points, carried by `by`.
  bool carriedBy(int id, int by) const {
    const Unit& u = un(id);
    for (int occ : battle.occupant)
      if (occ == id) return false;  // a carried unit holds no square
    return !u.alive && u.cell == kNoCell && u.hp > 0 && u.carriedBy == by && un(by).carrying == id && battle.exists(u);
  }
  // Passes the turn to the other side and back (red does nothing); `n` full rounds.
  void pass(int n = 1) {
    for (int i = 0; i < n; ++i) {
      act(Action::endTurn());
      act(Action::endTurn());
    }
  }
};

// Blue: soldier 0 (1,1), archer 1 (2,1), soldier 2 (1,2), soldier 3 (0,2). Red: soldier 4 (4,1).
inline RescueWorld line(RescueMode mode = RescueMode::Carry) {
  return RescueWorld(mode, {{"soldier", 0, 1, 1, false}, {"archer", 0, 2, 1, false}, {"soldier", 0, 1, 2, false},
                            {"soldier", 0, 0, 2, false}, {"soldier", 1, 4, 1, false}});
}

struct XY { int x, y; };

// A second move type that can only enter water (everybody else may wade), so a unit of the "boat" type cannot stand
// on land. Used where a carried unit has to be put down somewhere other than its carrier's square.
struct BoatWorld : RescueWorld {
  BoatWorld(const std::vector<UnitSpec>& units, const std::vector<XY>& water, const std::vector<const char*>& ids,
            RescueMode mode = RescueMode::Carry)
      : RescueWorld(mode, withoutBoats(units), open()) {
    db.moveTypes.push_back({"boat", "Boat"});
    for (TerrainDef& t : db.terrains) t.moveCost.push_back(0);
    for (StructureDef& s : db.structures) s.moveCost.push_back(-1);
    TerrainDef sea;
    sea.id = sea.name = "water";
    sea.moveCost = {1, 1};  // feet may wade, a boat may not leave the water
    db.terrains.push_back(sea);
    UnitTypeDef boat = db.unitTypes[0];
    boat.id = boat.name = "boat";
    boat.moveType = 1;
    boat.tags = {"soldier"};  // the sword's versus table knows this tag
    db.unitTypes.push_back(boat);
    map = fixture::makeMap(db, open(), units);
    for (size_t i = 0; i < ids.size(); ++i) map.units[i].id = ids[i];
    for (const XY& c : water) map.terrain[(size_t)(c.y * map.width + c.x)] = db.findTerrain("water");
  }
  static std::vector<UnitSpec> withoutBoats(const std::vector<UnitSpec>& units) {
    std::vector<UnitSpec> out;
    for (const UnitSpec& u : units)
      if (std::string(u.type) != "boat") out.push_back(u);
    return out;
  }
};

}  // namespace rescuetest
