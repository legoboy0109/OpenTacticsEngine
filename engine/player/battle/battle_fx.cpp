// Transient overlays: floating numbers, banners, level-up panel.
#include <algorithm>
#include <cmath>

#include "battle/battle_draw.h"
#include "ui/text.h"

namespace ote {

void drawFloats(Renderer& r, const DrawCtx& c, bool screenSpace) {
  int cx = c.cam.ix(), cy = c.cam.iy();
  for (const FloatText& f : c.view.floats) {
    if (f.screen != screenSpace) continue;
    float k = f.age / f.life;
    int rise = screenSpace ? 0 : (int)(f.age * 16);  // panel floats stay put on a backing plate
    int x = (int)f.x - (screenSpace ? 0 : cx), y = (int)f.y - (screenSpace ? 0 : cy) - rise;
    if (!screenSpace && c.raised()) {
      // The float belongs to the square under it: start above the unit standing
      // there, keeping the few pixels by which stacked floats are spread.
      int t = c.layout.tile;
      float row = std::floor((f.y + t * 0.5f) / t);
      int sx, sy;
      c.toScreen(f.x, (row + 0.5f) * t, sx, sy);
      x = sx;
      y = sy - c.proj->unitW() - c.layout.lh + (int)(f.y - row * t) - rise;
    }
    Color col = f.color;
    if (k > 0.7f) col.a = (uint8_t)(255 * (1 - (k - 0.7f) / 0.3f));  // fade out at the end
    int w = textW(r, f.text);
    if (!screenSpace) {  // keep the text inside the map viewport
      x = std::max(2 + w / 2, std::min(x, c.layout.vw - 2 - w / 2));
      y = std::max(1, y);
    }
    if (screenSpace) r.fillRect(x - w / 2 - 2, y - 1, w + 4, c.layout.lh + 2, Color(10, 12, 24, (uint8_t)(col.a * 220 / 255)));
    drawTextShadow(r, x - w / 2, y, f.text, col);
  }
}

void drawBanner(Renderer& r, const Layout& L, const std::string& text, Color color, int scale,
                const std::string& sub) {
  // Long text first shrinks (down to scale 1), then wraps onto several lines.
  int maxW = std::max(40, L.vw - 16);
  int sc = fitScale(r, text, maxW, scale);
  std::vector<std::string> lines = wrapText(r, text, maxW, sc);
  if (lines.empty()) lines.push_back("");
  if (lines.size() > 3) lines.resize(3);
  std::vector<std::string> subLines;
  if (!sub.empty()) {
    subLines = wrapText(r, sub, maxW);
    if (subLines.size() > 2) subLines.resize(2);
  }
  int lh = r.lineHeight(sc), slh = r.lineHeight();
  int h = (int)lines.size() * lh + 16 + (subLines.empty() ? 0 : (int)subLines.size() * slh + 4);
  int y = (L.vh - h) / 2;
  r.fillRect(0, y, L.vw, h, withAlpha(Color(0, 0, 0), 150));
  r.fillRect(0, y + 2, L.vw, h - 4, withAlpha(color, 235));
  Color tc = readableOn(color);
  int ty = y + 8;
  for (const std::string& ln : lines) {
    drawTextCentered(r, L.vw / 2, ty, fitText(r, ln, maxW, sc), tc, sc);
    ty += lh;
  }
  ty += 4;
  for (const std::string& ln : subLines) {
    drawTextCentered(r, L.vw / 2, ty, ln, withAlpha(tc, 200));
    ty += slh;
  }
}

void drawLevelUp(Renderer& r, const Layout& L, const LevelUpPanel& p) {
  if (!p.active) return;
  int maxW = std::max(60, L.vw - 40);
  std::string title = fitText(r, p.title, maxW);
  std::string foot = p.autoContinue ? "" : "Confirm to continue";
  int rh = L.row, tw = std::max(textW(r, title), textW(r, foot));
  for (const std::string& s : p.lines) tw = std::max(tw, textW(r, s) + 16);
  int w = std::min(tw + 24, L.vw - 8), rows = (int)p.lines.size();
  int h = 16 + rh + 2 + rows * rh + (foot.empty() ? 6 : rh + 8);
  int x = (L.vw - w) / 2, y = (L.vh - h) / 2;
  r.fillRect(x + 3, y + 3, w, h, Color(0, 0, 0, 120));
  r.fillRect(x, y, w, h, Color(26, 30, 52, 250));
  r.drawRect(x, y, w, h, Color(255, 220, 90));
  r.drawRect(x + 2, y + 2, w - 4, h - 4, Color(120, 100, 40));
  drawTextCentered(r, x + w / 2, y + 8, title, Color(255, 230, 120));
  for (int i = 0; i < rows; ++i)
    drawTextCentered(r, x + w / 2, y + 8 + rh + 8 + i * rh, fitText(r, p.lines[i], w - 12), Color(190, 255, 190));
  if (!foot.empty()) drawTextCentered(r, x + w / 2, y + h - rh - 4, foot, Color(150, 158, 190));
}

}  // namespace ote
