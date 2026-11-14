// Screen layout of the battle scene and the scrolling camera.
#pragma once
#include <algorithm>
#include <cmath>

#include "platform/platform.h"

namespace ote {

struct Layout {
  int W = 480, H = 270;
  int tile = 24;
  int lh = 8;        // line height of the active font
  int row = 10;      // panel/list row pitch (lh + 2)
  int panelW = 128;  // info panel on the right
  int hintH = 11;    // one-line hint strip at the bottom
  int vw = 352, vh = 259;  // map viewport (left, above the hint strip)
  int fontKey = 0;   // what the metrics were computed from (see sameFont)

  // Everything is sized from the active font: the panel is at least its
  // classic 128px, wider when the font needs room for ~15 characters, but
  // never more than half the screen so the map stays usable.
  static Layout make(int W, int H, int tile, const Renderer& r) {
    Layout l;
    l.W = W;
    l.H = H;
    l.tile = std::max(8, tile);
    l.lh = r.lineHeight();
    l.row = l.lh + 2;
    l.hintH = l.lh + 3;
    int digit = std::max(1, r.textWidth("0000000000") / 10);
    int wide = std::max(digit, r.textWidth("M") * 3 / 4);
    l.panelW = std::max(std::min(128, W * 3 / 8), 15 * std::max(digit, wide) + 8);
    l.panelW = std::min(l.panelW, W / 2);
    l.vw = W - l.panelW;
    l.vh = H - l.hintH;
    l.fontKey = fontKeyOf(r);
    return l;
  }
  static int fontKeyOf(const Renderer& r) { return r.lineHeight() * 1000 + r.textWidth("0123456789MW"); }
  bool stale(int w, int h, const Renderer& r) const { return w != W || h != H || fontKeyOf(r) != fontKey; }

  // Top of the panel's content (below the header bar).
  int panelTop() const { return lh + 4 + 3; }
};

// Camera in map pixels (top-left of the viewport). Maps smaller than the
// viewport are centred (negative offset).
struct Camera {
  float x = 0, y = 0;
  bool placed = false;

  int ix() const { return (int)std::floor(x + 0.5f); }
  int iy() const { return (int)std::floor(y + 0.5f); }

  static float axisTarget(float cur, float focus, int view, int map, int margin) {
    if (map <= view) return -(view - map) / 2.0f;
    float t = cur;
    if (focus < cur + margin) t = focus - margin;
    else if (focus > cur + view - margin) t = focus - view + margin;
    return std::max(0.0f, std::min(t, (float)(map - view)));
  }

  // Puts (fx, fy) in the middle of the viewport at once (views whose world
  // box is mostly empty space around the map start like this).
  void centre(float fx, float fy, const Layout& L, int mapW, int mapH) {
    x = mapW <= L.vw ? -(L.vw - mapW) / 2.0f : std::max(0.0f, std::min(fx - L.vw / 2.0f, (float)(mapW - L.vw)));
    y = mapH <= L.vh ? -(L.vh - mapH) / 2.0f : std::max(0.0f, std::min(fy - L.vh / 2.0f, (float)(mapH - L.vh)));
    placed = true;
  }

  // Keeps (fx, fy) inside the viewport (with a margin), easing quickly.
  void follow(float fx, float fy, const Layout& L, int mapW, int mapH, double dt) {
    int margin = L.tile * 2;
    float tx = axisTarget(x, fx, L.vw, mapW, margin), ty = axisTarget(y, fy, L.vh, mapH, margin);
    if (!placed) { x = tx; y = ty; placed = true; return; }
    float k = std::min(1.0f, (float)dt * 16.0f);
    x += (tx - x) * k;
    y += (ty - y) * k;
    if (std::fabs(tx - x) < 0.3f) x = tx;
    if (std::fabs(ty - y) < 0.3f) y = ty;
  }
};

}  // namespace ote
