// The squares a weapon's area attack covers, on both grid kinds (docs/SRPG.md "Area attacks and line fire"). ONE function does it for
// every shape, and the rules, the forecast, the computer opponent and the player's highlight all call it, so they cannot disagree.
#pragma once
#include <vector>

#include "core/data/weapon_area.h"
#include "core/grid/grid.h"

namespace ote {

// The direction (Grid::neighbor numbering) in which `to` lies on a straight line from `from` (square grids: the 4 orthogonal directions;
// hexagons: the 6 hexagon directions), or -1 when it does not (or the squares are the same). `steps` (optional) gets the distance.
int straightDirection(const Grid& g, Cell from, Cell to, int* steps = nullptr);

// The squares an attack from `from` aimed at `target` covers, ascending, without duplicates; the target square is always among them
// (when it is valid). Shapes: Radius = every square within `radius` of the target; Burst = the target and its direct neighbours;
// Cross = the target and `radius` squares in each direction of the grid; Ring = every square within `radius` of the target except the target
// itself (the squares around it); Field = every square of the map (where it is aimed does not matter); Line = `length` squares along the line from `from` through
// `target`, the target square first and then the squares behind it (the beam pierces: nothing stops it but the map edge).
// A Line aimed at a square that is not on a straight line from `from` covers nothing. AreaShape::None covers just the target.
void areaCells(const Grid& g, Cell from, Cell target, const AreaDef& area, std::vector<Cell>& out);

}  // namespace ote
