// Putting the 2D layer's rectangles on whole device pixels (platform/common/pixel_snap.h).
#include <cmath>

#include "platform/common/pixel_snap.h"
#include "test.h"

using namespace ote;

namespace {
OutputMap map(float kx, float ky, float ox, float oy) {
  OutputMap m;
  m.scaleX = kx;
  m.scaleY = ky;
  m.offX = ox;
  m.offY = oy;
  m.known = true;
  return m;
}

// The device edges a snapped rectangle really gets (what the backend computes from the logical numbers it is handed).
void deviceEdges(const OutputMap& m, int x, int y, int w, int h, double& x0, double& y0, double& x1, double& y1) {
  float fx, fy, fw, fh;
  snapRect(m, x, y, w, h, fx, fy, fw, fh);
  x0 = (double)fx * m.scaleX + m.offX;
  y0 = (double)fy * m.scaleY + m.offY;
  x1 = ((double)fx + fw) * m.scaleX + m.offX;
  y1 = ((double)fy + fh) * m.scaleY + m.offY;
}

bool whole(double v) { return std::fabs(v - std::round(v)) < 1e-3; }
}  // namespace

TEST(pixel_snap_leaves_whole_scales_alone) {
  float x, y, w, h;
  snapRect(map(3, 3, 0, 0), 5, 7, 24, 24, x, y, w, h);
  CHECK(x == 5 && y == 7 && w == 24 && h == 24);
  snapRect(map(1, 1, 12, 0), 5, 7, 24, 24, x, y, w, h);  // a whole offset moves nothing either
  CHECK(x == 5 && y == 7 && w == 24 && h == 24);
  snapRect(OutputMap(), 5, 7, 24, 24, x, y, w, h);  // mapping not known yet: unchanged
  CHECK(x == 5 && y == 7 && w == 24 && h == 24);
}

TEST(pixel_snap_puts_every_edge_on_a_whole_device_pixel) {
  const float scales[] = {0.833f, 1.5f, 2.604f, 2.744f, 3.333f, 4.833f};
  for (float k : scales)
    for (int x = 0; x < 60; x += 7)
      for (int w : {1, 5, 24, 31}) {
        double x0, y0, x1, y1;
        deviceEdges(map(k, k, 3, 0), x, x + 2, w, w, x0, y0, x1, y1);
        CHECK(whole(x0) && whole(y0) && whole(x1) && whole(y1));
        CHECK(x1 >= x0 + 1 - 1e-3 && y1 >= y0 + 1 - 1e-3);  // never vanishes
      }
}

TEST(pixel_snap_keeps_neighbours_watertight) {
  // 10 squares of 24 logical pixels side by side: each one's right edge is the next one's left edge, at any scale.
  const float scales[] = {1.5f, 1.875f, 2.667f, 2.744f, 3.889f};
  for (float k : scales) {
    const OutputMap m = map(k, k, 0, 0);
    double prevRight = 0;
    for (int i = 0; i < 10; ++i) {
      double x0, y0, x1, y1;
      deviceEdges(m, i * 24, 0, 24, 24, x0, y0, x1, y1);
      if (i > 0) CHECK(std::fabs(x0 - prevRight) < 1e-3);
      prevRight = x1;
    }
  }
}

TEST(pixel_snap_keeps_pixel_centres_off_the_edges) {
  // The cause of the dark seams: at 1.5x an edge fell exactly on a pixel centre (x.5). After snapping, no edge is within a
  // quarter pixel of any centre.
  const OutputMap m = map(1.5f, 1.5f, 0, 0);
  for (int y = 0; y < 40; ++y) {
    double x0, y0, x1, y1;
    deviceEdges(m, 0, y, 24, 24, x0, y0, x1, y1);
    CHECK(std::fabs(std::fabs(y0 - std::floor(y0)) - 0.5) > 0.25);
    CHECK(std::fabs(std::fabs(y1 - std::floor(y1)) - 0.5) > 0.25);
  }
}

namespace {
// What SDL's software renderer does with the snapped numbers: float multiply by the scale, then position and size each
// truncated to an int on its own (SW_QueueCopy / SW_QueueFillRects). Gives the whole-device-pixel edges it ends up with.
void truncatedEdges(const OutputMap& m, int x, int w, int& left, int& right) {
  float fx, fy, fw, fh;
  snapRect(m, x, 0, w, 1, fx, fy, fw, fh);
  const int px = (int)(fx * m.scaleX), pw = (int)(fw * m.scaleX);
  left = px;
  right = px + pw;
}
// How many squares of a row end up with an edge other than the ideal one: floor(pos * k + off + 0.5) - off.
int misplaced(const OutputMap& m) {
  int bad = 0;
  for (int x = -90; x < 520; x += 24) {  // some squares are partly left of the picture
    int l, r;
    truncatedEdges(m, x, 24, l, r);
    const int wantL = (int)std::floor((double)x * m.scaleX + m.offX + 0.5) - (int)m.offX;
    const int wantR = (int)std::floor((double)(x + 24) * m.scaleX + m.offX + 0.5) - (int)m.offX;
    if (l != wantL || r != wantR) ++bad;
  }
  return bad;
}
}  // namespace

TEST(pixel_snap_survives_a_renderer_that_truncates) {
  // The browser's software renderer (a browser with no WebGL) cuts (x * k) and (w * k) down to ints separately: without the
  // nudge, neighbouring squares came out one pixel apart at some edges (full-length dark lines across the map).
  const float scales[] = {1.5f, 1.875f, 2.45417f, 2.744f, 3.06875f, 3.68125f, 4.833f, 0.833f};
  int withoutNudge = 0;
  for (float k : scales) {
    OutputMap m = map(k, k, 0, 27);
    withoutNudge += misplaced(m);
    m.nudge = 1.0f / 32;
    CHECK(misplaced(m) == 0);
  }
  CHECK(withoutNudge > 0);  // only worth anything if the trouble is real
}
