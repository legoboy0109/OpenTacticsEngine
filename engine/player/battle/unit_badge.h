// Unit badges (docs/GRAPHICS.md "Unit badges"): a small picture from data/graphics.json "badges" drawn on the map sprite of the units that have a tag (a boss
// marker). One function for every kind of view: the caller knows where the unit's box is on screen.
#pragma once
#include "battle/battle_draw.h"

namespace ote {

// True when the badge applies to the unit: its character's or its unit type's tag, and the side rule (the side the unit is drawn on).
bool badgeApplies(const DrawCtx& c, const BadgeDef& badge, const Unit& u);

// Draws the badges of the unit over its box (left, top, w, h on screen; w is one square's width on screen: the badge's size and offset are
// scaled by w / the game's tile size, so it grows and shrinks with the map zoom and never with the UI scale). `alpha` 0..1 follows the unit's fade.
// Nothing in a game without badges. The caller draws only units the viewer sees (a unit hidden by fog of war is not drawn, so neither is its badge).
void drawUnitBadges(Renderer& r, const DrawCtx& c, const Unit& u, int left, int top, int w, int h, float alpha);

}  // namespace ote
