// The commander's block of the side panel (docs/COMMANDERS.md "Player"): the commander's name with "meter/top", a bar with a tick at the price of each
// power, and while a power runs its name with the turns left. Sized from the font like the rest of the panel, so it fits UI scale 100 and 150.
#include <algorithm>

#include "battle/battle_draw.h"
#include "battle/commander_text.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {

namespace {
const Color kLabel(150, 158, 190), kText(228, 230, 240), kRule(60, 66, 96);
const Color kBarBack(36, 40, 62), kBarFill(110, 150, 230), kBarReady(120, 230, 150), kBarActive(255, 205, 90), kTick(235, 238, 250);
}  // namespace

int drawCommanderMeter(Renderer& r, const DrawCtx& c, int side, int x, int& y, int w) {
  const Battle& b = c.battle;
  const CommanderDef* d = b.commanderOf(side);
  if (!d) return 0;
  const int top = y;
  const int lh = c.layout.lh, rh = c.layout.row;
  r.fillRect(x, y + 2, w, 1, kRule);
  y += 6;
  const bool active = b.activePower(side) >= 0;
  const bool ready = anyPowerReady(b, side);  // a power could be used now (so only on the side's own turn)
  // "Ada          40/100": the name gives way to the numbers.
  const std::string value = meterText(b, side);
  const int vw = textW(r, value);
  const int gap = std::max(4, lh / 2);
  r.drawText(x, y, fitText(r, d->name, w - vw - gap).c_str(), kLabel);
  r.drawText(x + w - vw, y, value.c_str(), active ? kBarActive : ready ? kBarReady : kText);
  y += rh;
  // The bar and the power prices on it.
  const int bh = std::max(4, lh / 2);
  r.fillRect(x, y, w, bh, kBarBack);
  const int fill = d->meterMax > 0 ? (int)((long long)w * b.meterOf(side) / d->meterMax) : 0;
  r.fillRect(x, y, std::min(w, fill), bh, active ? kBarActive : ready ? kBarReady : kBarFill);
  for (size_t i = 0; i < d->powers.size(); ++i) {
    const int cost = b.powerCost(side, (int)i);
    const int tx = x + std::min(w - 1, (int)((long long)w * cost / d->meterMax));
    r.fillRect(tx, y - 1, 1, bh + 2, kTick);
  }
  y += bh + 3;
  if (active) {  // "Power  Rally 1": the name and the turns it still has
    const CommanderPower& p = d->powers[(size_t)b.activePower(side)];
    const std::string left = intStr(b.powerTurnsLeft(side));
    const int lw = textW(r, left);
    r.drawText(x, y, fitText(r, std::string(CommanderWords::kPowerWord) + ": " + p.name, w - lw - gap).c_str(), kBarActive);
    r.drawText(x + w - lw, y, left.c_str(), kBarActive);
    y += rh;
  }
  return y - top;
}

}  // namespace ote
