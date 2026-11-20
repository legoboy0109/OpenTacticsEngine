// The other sides' commanders in the side panel (docs/COMMANDERS.md "Player"). Rule: every OTHER side with a commander gets one slim block under the viewed side's
// own, in turn order starting with the side that acts after the viewed one, at most two of them (a battle of many sides shows the next to act first), and only while
// they fit above the cell and unit rows. It reads the battle's own numbers, so fog hides nothing: a meter is not information about the map. A name in the side's
// colour, "meter/top" (dropped when the row is too narrow, as at UI scale 150), a thin bar with a tick per power price; while a power runs the bar turns amber and the
// number becomes "P<turns left>".
#include <algorithm>

#include "battle/battle_draw.h"
#include "battle/commander_text.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {

namespace {
const Color kBarBack(36, 40, 62), kBarActive(255, 205, 90), kTick(235, 238, 250), kText(228, 230, 240);
}  // namespace

int drawOtherCommanderMeters(Renderer& r, const DrawCtx& c, int viewed, int x, int& y, int w, int bottom) {
  const Battle& b = c.battle;
  const int n = (int)b.sides.size();
  const int top = y;
  const int lh = c.layout.lh, rh = c.layout.row;
  const int bh = std::max(3, lh / 3);
  int shown = 0;
  for (int k = 1; k < n && shown < 2; ++k) {
    const int s = (viewed + k) % n;
    const CommanderDef* d = b.commanderOf(s);
    if (!d || b.sides[(size_t)s].defeated) continue;
    if (y + rh + bh + 4 > bottom) break;  // no room: the unit rows below matter more
    const bool active = b.activePower(s) >= 0;
    std::string value = active ? "P" + intStr(b.powerTurnsLeft(s)) : meterText(b, s);
    const int gap = std::max(4, lh / 2);
    int vw = textW(r, value);
    if (textW(r, d->name) + vw + gap > w) {  // too tight: the bar alone says it
      value.clear();
      vw = 0;
    }
    static std::string lastLog;  // one [info] line per change (the windowless tests read it)
    const std::string logLine = "other meter: " + d->name + " " + meterText(b, s) + (active ? " power active" : "");
    if (logLine != lastLog) {
      lastLog = logLine;
      OTE_INFO("%s", logLine.c_str());
    }
    const Color sc = Color::rgb(b.sides[(size_t)s].color);
    r.drawText(x, y, fitText(r, d->name, w - vw - gap).c_str(), sc);
    if (vw) r.drawText(x + w - vw, y, value.c_str(), active ? kBarActive : kText);
    y += rh - 1;
    r.fillRect(x, y, w, bh, kBarBack);
    const int fill = d->meterMax > 0 ? (int)((long long)w * b.meterOf(s) / d->meterMax) : 0;
    r.fillRect(x, y, std::min(w, fill), bh, active ? kBarActive : sc);
    for (size_t i = 0; i < d->powers.size(); ++i) {
      const int tx = x + std::min(w - 1, (int)((long long)w * b.powerCost(s, (int)i) / d->meterMax));
      r.fillRect(tx, y - 1, 1, bh + 2, kTick);
    }
    y += bh + 3;
    ++shown;
  }
  return y - top;
}

}  // namespace ote
