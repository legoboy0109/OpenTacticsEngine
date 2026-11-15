// Helpers for the fog of war tests (docs/ECONOMY.md "Fog of war"): the miniature game of battle_fixture.h with
// the map's "fog" switched on. Sides: blue (0, human, team 0), red (1, ai, team 1); map legend of the fixture
// ('.' plain, '#' wall, 'f' forest, 'c' city, 'H' hq, 'F' factory). Unit types keep the default vision of 2
// (grid distance); tests change it on `db.unitTypes` before begin().
//   extra unit type "blind" (index 3): a soldier with vision 0
//   extra structure kind "tower" (index 7): 1 x 1, vision bonus 3, hides; "hut" (8): capturable, hides
//   extra terrain "hill" is not needed: tests set db.terrains[forest].visionBonus / hides themselves.
#pragma once
#include "story_fixture.h"

namespace fogtest {

using namespace ote;

enum { Tower = 7, Hut = 8 };  // db.structures indices (after the fixture's 7)

// The base class builds its map before this fixture adds the "blind" type, so it only gets the units that exist then.
inline std::vector<fixture::UnitSpec> filtered(const std::vector<fixture::UnitSpec>& units) {
  std::vector<fixture::UnitSpec> out;
  for (const fixture::UnitSpec& u : units)
    if (std::string(u.type) != "blind") out.push_back(u);
  return out;
}

struct FogWorld : story::StoryWorld {
  FogWorld(const std::vector<std::string>& rows, const std::vector<fixture::UnitSpec>& units,
           const std::vector<const char*>& ids = {})
      : StoryWorld(rows, filtered(units)) {
    map.fog = true;
    db.rules.capture = db.rules.production = db.rules.economy = true;
    db.rules.defeatLostHq = false;
    auto kind = [](const char* id) {
      StructureDef s;
      s.id = s.name = id;
      s.moveCost = {-1};
      return s;
    };
    StructureDef tower = kind("tower"); tower.visionBonus = 3; tower.hides = true;
    StructureDef hut = kind("hut"); hut.capturePoints = 20; hut.hides = true;
    // "blind": a soldier that sees only its own square (so a neighbour on plain ground can stay unseen).
    UnitTypeDef blind = db.unitTypes[0];
    blind.id = blind.name = "blind";
    blind.glyph = 'B';
    blind.vision = 0;
    blind.tags = {"soldier"};  // the sword's versus table knows this tag
    db.unitTypes.push_back(blind);
    db.structures.push_back(tower);
    db.structures.push_back(hut);
    // The map is built again now that "blind" exists (the base class built it before).
    map = fixture::makeMap(db, rows, units);
    for (size_t i = 0; i < ids.size(); ++i) map.units[i].id = ids[i];
    map.fog = true;
  }
  // The forest square becomes a hideout.
  void forestHides() { db.terrains[(size_t)db.findTerrain("forest")].hides = true; }
  void vision(const char* type, int v) { db.unitTypes[(size_t)db.findUnitType(type)].vision = v; }

  bool sees(int team, int x, int y) const { return battle.visibleTo(team, at(x, y)); }
  bool seesUnit(int team, int unit) const { return battle.unitVisibleTo(team, unit); }
  int unitIdAt(int x, int y) const { return battle.unitAt(at(x, y)); }
  Unit& unit(int x, int y) { return battle.units[(size_t)unitIdAt(x, y)]; }
  int structure(int x, int y) const { return battle.structureAt[(size_t)at(x, y)]; }
  // Blue and red end their turns: blue's next turn has begun.
  bool nextTurn() { return act(Action::endTurn()) && act(Action::endTurn()); }
  std::string refused(const Action& a) {
    std::string why;
    if (battle.validate(a, &why)) return "";
    return why.empty() ? "refused" : why;
  }
  std::vector<Event> all(EventType t) const {
    std::vector<Event> out;
    for (const Event& e : events)
      if (e.type == t) out.push_back(e);
    return out;
  }
  // Another side in the team named `team` (a team that no side has makes a new one), before begin().
  void addSide(const char* id, const char* team) {
    MapSideDef side;
    side.id = side.name = id;
    side.team = team;
    map.sides.push_back(side);
  }
  void addAlly(const char* team) { addSide("green", team); }
  void addUnit(const char* type, int side, int x, int y) {
    MapUnitDef u;
    u.type = db.findUnitType(type);
    u.side = side;
    u.x = x;
    u.y = y;
    u.stats = db.unitTypes[(size_t)u.type].stats;
    map.units.push_back(u);
  }
};

inline std::vector<std::string> field(int w = 12, int h = 3) {
  return std::vector<std::string>((size_t)h, std::string((size_t)w, '.'));
}

}  // namespace fogtest
