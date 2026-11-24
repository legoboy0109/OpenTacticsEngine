// How a weapon's attack reaches its targets: "fire" (direct, or along straight lines only) and "area" (the attack hits a whole patch
// of squares). Both are optional keys of data/weapons.json (docs/SRPG.md "Area attacks and line fire"); a weapon with neither is the
// single-target weapon every game had before. Rules: core/battle/area_attack.cpp; the squares of a shape: core/grid/area.h.
#pragma once

namespace ote {

enum class FireMode { Direct, Line };  // Line: the weapon only targets along the straight lines of the grid, and the first unit stops the shot
// None = a single-target weapon. Burst is a named shorthand for Radius 1. Ring = Radius without the aimed square itself; Field = every square of the
// map (docs/SRPG.md "Area attacks and line fire"). New shapes go at the END (the numbers are not saved, but keep the old ones stable anyway).
enum class AreaShape { None, Burst, Line, Radius, Cross, Ring, Field };

constexpr int kMaxAreaSize = 8;  // the biggest "radius" / "length" the loader accepts

struct AreaDef {
  AreaShape shape = AreaShape::None;
  int radius = 0;               // Radius, Cross, Ring (1..8); Burst is always 1; Field has none
  int length = 0;               // Line (1..8): the squares of the beam, the target square first
  bool friendlyFire = false;    // true: units of the attacker's own side (and allies) in the area are hit too
  int apExtra = 0;              // "apExtra": AP added to the weapon's cost for the area (action points only)
  int vertical = -1;            // "vertical" (height rules only): the squares more than this many levels above / below the aimed one are not covered; -1 = no limit
  bool on() const { return shape != AreaShape::None; }
};

}  // namespace ote
