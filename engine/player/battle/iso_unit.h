// One unit as the 2D isometric kind of view draws it (docs/VIEWS.md "Drawing"), shared by squares and hexagons: the
// caller works out where the unit stands, this draws the figure there.
#pragma once
#include <functional>

#include "battle/battle_draw.h"

namespace ote {

// Draws a flat shape (a diamond, a hexagon) filling the box x, y, w, h in the given colour.
using IsoShapeFn = std::function<void(int x, int y, int w, int h, Color col)>;

// The unit with its feet at (cx, foot), its spot on the ground at (cx, cy), pictures `uw` wide. With art and
// `unitMarker`, `marker` draws the small shape in the side's colour (dark rim first) under the feet. Without art an
// upright block with the unit's letter. Then the leader mark and the HP bar under the feet.
// `faceX`, `faceY`: where the edge of its square in the direction it looks is on the ground on screen (the facing marker; docs/SRPG.md "Facing"), or
// kNoFace for none.
constexpr int kNoFace = -1000000;
void drawIsoUnitFigure(Renderer& r, const DrawCtx& c, const Unit& u, const UnitView& v, float fade, int cx, int cy,
                       int foot, int uw, const IsoShapeFn& marker, int faceX = kNoFace, int faceY = kNoFace);

}  // namespace ote
