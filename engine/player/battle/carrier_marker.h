// The carrier marker and the panel row of carrying / pair up (docs/SRPG.md "Player UI > Rescue / pair up"): a small side-coloured tab at the top right corner
// of a unit that has someone (the leader mark sits at the top left). One function for every kind of view: the caller
// (top-down and hexagon unit, isometric figure, 3D billboard) knows where the unit's top right corner is on screen.
#pragma once
#include "battle/battle_draw.h"

namespace ote {

// True when `u` should wear the marker now: it has someone, and the view no longer draws that someone (an animation
// still sliding the unit onto its carrier shows no marker until it has gone in; a dropped one has none at all).
bool carrierMarked(const DrawCtx& c, const Unit& u);

// Draws the tab with its right edge at `right` and its top at `top`; `unitW` = the unit's picture width in pixels
// (the tab is a third of it, at least 4 x 5); `alpha` 0..1 follows the unit's fade.
void drawCarrierMarker(Renderer& r, const DrawCtx& c, const Unit& u, int right, int top, int unitW, float alpha);

// The info panel row "Carrying: Bram" / "With: Bram" of a unit that has someone (panel_rescue.cpp): draws at (x, y) in
// a row `w` wide and moves y down one row; does nothing for a unit with nobody or when the row would not fit.
void drawRescueRow(Renderer& r, const DrawCtx& c, const Unit& u, int x, int& y, int w);

}  // namespace ote
