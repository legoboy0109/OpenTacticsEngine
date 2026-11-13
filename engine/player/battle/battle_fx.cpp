// Transient overlays: floating numbers, banners, level-up panel.
#include <algorithm>

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
    Color col = f.color;
    if (k > 0.7f) col.a = (uint8_t)(255 * (1 - (k - 0.7f) / 0.3f));  // fade out at the end
    int w = textWidth(f.text);
    if (!screenSpace) {  // keep the text inside the map viewport
      x = std::max(2 + w / 2, std::min(x, c.layout.vw - 2 - w / 2));
      y = std::max(1, y);
    }
    if (screenSpace) r.fillRect(x - w / 2 - 2, y - 1, w + 4, 10, Color(10, 12, 24, (uint8_t)(col.a * 220 / 255)));
    drawTextShadow(r, x - w / 2, y, f.text, col);
  }
}

void drawBanner(Renderer& r, const Layout& L, const std::string& text, Color color, int scale,
                const std::string& sub) {
  int h = kGlyph * scale + 16 + (sub.empty() ? 0 : 12);
  int y = (L.vh - h) / 2;
  r.fillRect(0, y, L.vw, h, withAlpha(Color(0, 0, 0), 150));
  r.fillRect(0, y + 2, L.vw, h - 4, withAlpha(color, 235));
  Color tc = readableOn(color);
  int chars = L.vw / (kGlyph * scale) - 1;
  drawTextCentered(r, L.vw / 2, y + 8, fitText(text, chars), tc, scale);
  if (!sub.empty()) drawTextCentered(r, L.vw / 2, y + 8 + kGlyph * scale + 4, sub, withAlpha(tc, 200));
}

void drawLevelUp(Renderer& r, const Layout& L, const LevelUpPanel& p) {
  if (!p.active) return;
  int maxChars = std::max(12, L.vw / kGlyph - 6);
  std::string title = fitText(p.title, maxChars);
  std::string foot = p.autoContinue ? "" : "Confirm to continue";
  int chars = std::max((int)title.size(), (int)foot.size());
  for (const std::string& s : p.lines) chars = std::max(chars, (int)s.size() + 2);
  int w = chars * kGlyph + 24, rows = (int)p.lines.size();
  int h = 16 + 12 + rows * 10 + (foot.empty() ? 6 : 18);
  int x = (L.vw - w) / 2, y = (L.vh - h) / 2;
  r.fillRect(x + 3, y + 3, w, h, Color(0, 0, 0, 120));
  r.fillRect(x, y, w, h, Color(26, 30, 52, 250));
  r.drawRect(x, y, w, h, Color(255, 220, 90));
  r.drawRect(x + 2, y + 2, w - 4, h - 4, Color(120, 100, 40));
  drawTextCentered(r, x + w / 2, y + 8, title, Color(255, 230, 120));
  for (int i = 0; i < rows; ++i)
    drawTextCentered(r, x + w / 2, y + 24 + i * 10, p.lines[i], Color(190, 255, 190));
  if (!foot.empty()) drawTextCentered(r, x + w / 2, y + h - 14, foot, Color(150, 158, 190));
}

}  // namespace ote
