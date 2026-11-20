// How a weapon's attack reaches its targets: "fire" (direct, or along straight lines only) and "area" (the attack hits a whole patch
// of squares). Both are optional keys of data/weapons.json (docs/SRPG.md "Area attacks and line fire"); a weapon with neither is the
// single-target weapon every game had before. Rules: core/battle/area_attack.cpp; the squares of a shape: core/grid/area.h.
#pragma once

namespace ote {

enum class FireMode { Direct, Line };  // Line: the weapon only targets along the straight lines of the grid, and the first unit stops the shot
// None = a single-target weapon. Burst is a named shorthand for Radius 1.
enum class AreaShape { None, Burst, Line, Radius, Cross };

constexpr int kMaxAreaSize = 8;  // the biggest "radius" / "length" the loader accepts

struct AreaDef {
  AreaShape shape = AreaShape::None;
  int radius = 0;               // Radius (1..8), Cross (1..8); Burst is always 1
  int length = 0;               // Line (1..8): the squares of the beam, the target square first
  bool friendlyFire = false;    // true: units of the attacker's own side (and allies) in the area are hit too
  int apExtra = 0;              // "apExtra": AP added to the weapon's cost for the area (action points only)
  bool on() const { return shape != AreaShape::None; }
};

}  // namespace ote
