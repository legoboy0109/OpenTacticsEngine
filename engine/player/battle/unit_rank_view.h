// Unit ranks in the player (docs/SRPG.md "Unit ranks", "Player"): the badge on the map and the words for the unit details. Plain functions of the
// battle (and, for the badge, of what the view has played so far), so every kind of view draws the same thing.
#pragma once
#include <string>

#include "battle/battle_draw.h"

namespace ote {

// The rank the map shows for the unit: what the UnitRankUp events played so far say (UnitView::rank), else the unit's own. -1 = none (also in a game without a table).
int shownRank(const DrawCtx& c, const Unit& u);
// The badge text of that rank ("" = none: no rank yet, a rank whose "badge" is empty, or a game without a rank table).
std::string rankBadgeText(const DrawCtx& c, const Unit& u);
// Draws the badge as a tiny gold-on-dark tab with its right edge at `right` and its bottom at `bottom`; `unitW` is the unit's width on screen, which sizes it
// (it never grows with the UI scale: at most 4/9 of the unit wide, 3x5 pixel letters, or one pip per character when the letters do not fit). `alpha` 0..1 follows the unit's fade.
// One function for the four kinds of view: the caller knows where the unit's bottom right corner is on screen.
void drawRankBadge(Renderer& r, const DrawCtx& c, const Unit& u, int right, int bottom, int unitW, float alpha);

}  // namespace ote
