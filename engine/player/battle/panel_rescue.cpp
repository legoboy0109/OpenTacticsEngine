// The info panel row of a unit that has someone: "Carrying: Bram" / "With: Bram" (docs/SRPG.md "Player UI > Info
// panel"). panel.cpp (step U1's file) makes one call to this from drawUnitInfo.
#include "battle/carrier_marker.h"
#include "battle/rescue_menu.h"
#include "ui/text.h"

namespace ote {

void drawRescueRow(Renderer& r, const DrawCtx& c, const Unit& u, int x, int& y, int w) {
  const Battle& b = c.battle;
  // Fog: the name of somebody a unit of another team carries is not the viewer's to read.
  const bool hide = c.view.fogged() && b.teamOf(u) != c.view.viewerTeam();
  const std::string text = rescueRowText(b, u, hide);
  if (text.empty() || y > c.layout.vh - c.layout.lh) return;  // rows that do not fit are dropped, as everywhere
  const size_t colon = text.find(": ");
  const std::string label = text.substr(0, colon + 2), name = text.substr(colon + 2);
  const Color kLabel(150, 158, 190), kText(228, 230, 240);  // the panel's own colours (panel.cpp)
  r.drawText(x, y, fitText(r, label, w).c_str(), kLabel);
  const int lw = textW(r, label);
  if (lw < w) r.drawText(x + lw, y, fitText(r, name, w - lw).c_str(), kText);
  y += c.layout.row;
}

}  // namespace ote
