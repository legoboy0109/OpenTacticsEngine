// Screen layout of the battle scene and the scrolling camera.
#pragma once
#include <algorithm>
#include <cmath>

namespace ote {

struct Layout {
  int W = 480, H = 270;
  int tile = 24;
  int panelW = 128;  // info panel on the right
  int hintH = 11;    // one-line hint strip at the bottom
  int vw = 352, vh = 259;  // map viewport (left, above the hint strip)

  static Layout make(int W, int H, int tile) {
    Layout l;
    l.W = W;
    l.H = H;
    l.tile = std::max(8, tile);
    l.panelW = std::min(128, W * 3 / 8);
    l.vw = W - l.panelW;
    l.vh = H - l.hintH;
    return l;
  }
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
