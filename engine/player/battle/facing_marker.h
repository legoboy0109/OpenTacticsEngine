// The facing marker (docs/SRPG.md "Facing", "Player"): a small light arrowhead on the ground at the edge of a unit's square in the direction it looks, with
// a dark rim so it reads on any colour. The engine's unit pictures have walking frames but no standing ones for each direction, so facing is shown by
// this marker, drawn by every kind of view (top-down squares and hexagons, 2D isometric, 3D isometric, full 3D) from the same function: the caller knows
// where the unit's ground spot is on screen and where the edge of its square in that direction is.
#pragma once
#include "battle/battle_draw.h"

namespace ote {

// Where the edge of a unit's square lies from its middle, in flat map pixels (the pixels BattleView animates units in), for direction `dir` of the grid:
// half a step of the grid toward that neighbour. The views project (spot + this) like any other map point to find the arrow's tip on screen.
void facingEdgeVector(const Grid& g, int dir, int tile, float& fx, float& fy);

// The direction a unit's marker shows: what the events have played so far (UnitView::facing), else the unit's own; -1 = none (a game without facing).
int shownFacing(const DrawCtx& c, const Unit& u);

// Draws the arrowhead for a unit standing at screen point (cx, cy) whose square's edge in its facing direction is at (ex, ey) on screen. `alpha` 0..1
// follows the unit's fade. Nothing is drawn in a game without facing, for a unit with no facing, or when the two points are nearly the same.
void drawFacingMarker(Renderer& r, const DrawCtx& c, const Unit& u, int cx, int cy, int ex, int ey, float alpha);

// The common case, the top-down view: the unit's middle at (cx, cy) on screen, `tile` pixels wide.
void drawFacingMarkerFlat(Renderer& r, const DrawCtx& c, const Unit& u, int cx, int cy, int tile, float alpha);

}  // namespace ote
