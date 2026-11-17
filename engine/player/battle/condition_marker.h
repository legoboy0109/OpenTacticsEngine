// The condition marker (docs/SRPG.md section 9 "Player"): a small dark tab with the short text tag of the unit's first condition ("Po"),
// and a "+" when it has more, at the bottom left corner of a unit (the leader mark sits at the top left, the carrier tab at the top
// right). The engine has no small pictures for units, so a condition is shown by its tag. One function for every kind of view: the
// caller (top-down and hexagon unit, isometric figure, 3D billboard) knows where the unit's bottom left corner is on screen.
#pragma once
#include "battle/battle_draw.h"

namespace ote {

// The tag text the unit wears now ("" = none). It follows the events the view has played (UnitView::conds), so it appears with the
// "Poison!" float and goes with "Poison ended".
std::string conditionMarkerText(const DrawCtx& c, const Unit& u);

// Draws the tab with its left edge at `left` and its bottom at `bottom`; `alpha` 0..1 follows the unit's fade.
void drawConditionMarker(Renderer& r, const DrawCtx& c, const Unit& u, int left, int bottom, float alpha);

}  // namespace ote
