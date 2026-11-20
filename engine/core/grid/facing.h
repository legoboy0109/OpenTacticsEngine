// Facing on both grid kinds (docs/SRPG.md "Facing"): the names of the directions, which way a square lies from another, and the arc (front, side or
// back) an attack comes from relative to a unit's facing. Pure grid code; the rules that use it are core/battle/facing_rules.cpp.
//
// A facing is a direction number of Grid::neighbor: squares 0..3 = north, east, south, west; hexagons 0..5 in the order of docs/HEX.md.
#pragma once
#include <string>

#include "core/grid/grid.h"

namespace ote {

enum Arc { kArcFront = 0, kArcSide = 1, kArcBack = 2 };  // the values formulas read as `arc`

// The names a map writes: squares "north" "east" "south" "west"; pointy hexagons "northeast" "east" "southeast" "southwest" "west" "northwest";
// flat hexagons "north" "northeast" "southeast" "south" "southwest" "northwest" (direction 0, 1, 2 ... in that order). "" for a direction out of range.
std::string directionName(const Grid& g, int dir);
// The direction a name stands for, -1 when it is none of the grid's directions.
int directionFromName(const Grid& g, const std::string& name);

// The directions in which `to` lies seen from `from`: one, or two when it lies exactly between two (a square on a true diagonal; a hexagon exactly
// on the line between two directions), in ascending order; none when the cells are the same. Squares: the dominant axis; hexagons: the nearest of
// the six directions by the straight line between the cells' middles. Returns how many were written to `out`.
int directionsToward(const Grid& g, Cell from, Cell to, int out[2]);
// The one direction to turn to look at `to` from `from`: the first of directionsToward (the lower number on a tie), -1 for the same cell.
int directionToward(const Grid& g, Cell from, Cell to);

// The arc of a direction `dir` (as seen from the unit) against a facing. Squares: front = the facing, back = the opposite, side = the other two.
// Hexagons: front = the facing and the two directions next to it, back = the opposite one, side = the remaining two.
int arcOfDirection(const Grid& g, int facing, int dir);
// The arc an attack from the square `attacker` comes from against a unit on `defender` that faces `facing`. An attacker not on a straight line
// takes the dominant direction (directionsToward); on a tie the MILDER of the two arcs counts (front before side before back), so a diagonal
// attacker is never behind a unit it has also in front of or beside it. Same square: front.
int arcOf(const Grid& g, Cell defender, int facing, Cell attacker);

}  // namespace ote
