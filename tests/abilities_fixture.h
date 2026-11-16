// Helpers for the ability and summon tests (docs/SRPG.md section 4; step A1): the miniature game of fog_fixture.h
// on a square map or on either hexagon layout, with or without fog, and abilities written in code.
//   stats (index): hp 0, atk 1, def 2, mov 3.   soldier hp10 atk5 def2 mov3, archer hp8 atk4 def1 mov3, tank hp20,
//   "blind" (a soldier that sees only its own square). Side 0 blue (human) moves first, side 1 red.
#pragma once
#include "fog_fixture.h"

namespace abtest {

using namespace ote;
using fixture::UnitSpec;

enum Kind { Square, Pointy, Flat };
const Kind kKinds[] = {Square, Pointy, Flat};
inline const char* kindName(Kind k) { return k == Square ? "square" : k == Pointy ? "pointy hexagons" : "flat hexagons"; }

inline Grid gridOf(Kind k, int w, int h) {
  return k == Square ? Grid(GridType::Square, w, h, HexLayout::Pointy)
                     : Grid(GridType::Hex, w, h, k == Flat ? HexLayout::Flat : HexLayout::Pointy);
}

// The ability "id" with its target, effect and amount (push squares / heal HP) set; tests change the rest.
inline AbilityDef ability(const char* id, AbilityTarget target, AbilityEffect effect, int amount = 0) {
  AbilityDef d;
  d.id = d.name = id;
  d.target = target;
  d.effect = effect;
  d.amount = amount;
  return d;
}

struct AbWorld : fogtest::FogWorld {
  // `rows` give the map's size (and its walls); `kind` the grid; `fogOn` the map's fog.
  AbWorld(Kind kind, bool fogOn, const std::vector<UnitSpec>& units, const std::vector<std::string>& rows = fogtest::field(9, 9),
          const std::vector<const char*>& ids = {})
      : FogWorld(rows, units, ids) {
    map.fog = fogOn;
    if (kind != Square) {
      db.gridType = map.gridType = GridType::Hex;
      db.hexLayout = map.hexLayout = kind == Flat ? HexLayout::Flat : HexLayout::Pointy;
    }
  }
  // Adds an ability and gives it to a unit type (before begin()); returns its index.
  int give(const char* type, const AbilityDef& d) {
    int at = -1;
    for (size_t i = 0; i < db.abilities.size(); ++i)
      if (db.abilities[i].id == d.id) at = (int)i;
    if (at < 0) {
      db.abilities.push_back(d);
      at = (int)db.abilities.size() - 1;
    }
    db.unitTypes[(size_t)db.findUnitType(type)].abilities.push_back(at);
    return at;
  }
  // A summon ability's unit type is resolved by the loader; tests name it by id here.
  static AbilityDef summon(const char* id, int type, int limit = 1) {
    AbilityDef d = ability(id, AbilityTarget::Cell, AbilityEffect::Summon);
    d.summon.type = type;
    d.summon.limit = limit;
    d.minRange = 1;
    d.maxRange = 2;
    return d;
  }
  const Unit& un(int id) const { return battle.units[(size_t)id]; }
  Unit& mut(int id) { return battle.units[(size_t)id]; }
  bool stands(int id, Cell c) const { return un(id).alive && un(id).cell == c && battle.occupant[(size_t)c] == id; }
  Cell cellOf(int id) const { return un(id).cell; }
  bool use(int unit, int ab, int target, Cell cell = kNoCell) { return act(Action::useAbility(unit, ab, target, cell)); }
  // The reason the ability is refused ("" = allowed).
  std::string why(int unit, int ab, int target, Cell cell = kNoCell) const {
    std::string got;
    if (battle.validate(Action::useAbility(unit, ab, target, cell), &got)) return "";
    return got.empty() ? "refused" : got;
  }
  // The last events of one type, newest last.
  std::vector<Event> of(EventType t) const { return all(t); }
  // Ends blue's and red's turns: blue's next turn has begun.
  void nextRound() { CHECK(nextTurn()); }
};

// The cell one step from (x, y) in direction `dir`, as x, y (the test sets units up by coordinates).
struct XY {
  int x, y;
};
inline XY step(const Grid& g, XY from, int dir) {
  Cell c = g.neighbor(g.at(from.x, from.y), dir);
  return {g.x(c), g.y(c)};
}

}  // namespace abtest
