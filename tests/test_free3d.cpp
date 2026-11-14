// Geometry of the "3d" (full 3D) kind of view (docs/VIEWS.md).
#include <cmath>

#include "core/view/free3d.h"
#include "gfx3d/math3d.h"
#include "test.h"

using namespace ote;

namespace {
Free3DGeometry cam(int w, int h, int levels, float yaw = 0, float pitch = 0.87f, float distance = 11) {
  Free3DGeometry g;
  g.gridW = w;
  g.gridH = h;
  g.levels = levels;
  g.yaw = yaw;
  g.pitch = pitch;
  g.distance = distance;
  return g;
}
}  // namespace

TEST(free3d_default_camera_looks_north_from_the_south) {
  Free3DGeometry g = cam(8, 8, 0);
  float ax, ay, bx, by;
  CHECK(g.project(4, 4, 0, ax, ay));  // the focus: the middle of the picture
  CHECK(std::fabs(ax - g.viewW * 0.5f) < 0.01f && std::fabs(ay - g.viewH * 0.5f) < 0.01f);
  g.project(5, 4, 0, bx, by);  // grid x grows to the right
  CHECK(bx > ax + 5 && std::fabs(by - ay) < 0.01f);
  g.project(4, 3, 0, bx, by);  // grid y shrinks up the screen
  CHECK(by < ay - 5 && std::fabs(bx - ax) < 0.01f);
  g.project(4, 4, 2, bx, by);  // higher ground is higher on screen
  CHECK(by < ay - 5);
}

TEST(free3d_projection_is_the_one_the_3d_layer_draws_with) {
  const float yaws[] = {0, 0.6f, 2.2f, -1.3f};
  for (float yaw : yaws) {
    Free3DGeometry g = cam(9, 6, 3, yaw, 0.6f, 14);
    g.focusX = 1.5f;
    g.focusY = 0.5f;
    g.focusZ = -1;
    float ex, ey, ez;
    g.eye(ex, ey, ez);
    Mat4 m = Mat4::perspective(g.fovY, (float)g.viewW / (float)g.viewH, 0.3f, 100) *
             Mat4::lookAt(Vec3(ex, ey, ez), Vec3(g.focusX, g.focusY, g.focusZ), Vec3(0, 1, 0));
    const float spots[5][3] = {{0, 0, 0}, {3, 1, -2}, {-4, 0, 2.5f}, {1.5f, 0.5f, -1}, {4.5f, 1.5f, 3}};
    for (const float* s : spots) {
      Vec4 c = m * Vec4(s[0], s[1], s[2], 1);
      float px = (c.x / c.w + 1) * 0.5f * g.viewW, py = (1 - c.y / c.w) * 0.5f * g.viewH, sx, sy;
      CHECK(g.projectPoint(s[0], s[1], s[2], sx, sy));
      CHECK(std::fabs(px - sx) < 0.02f && std::fabs(py - sy) < 0.02f);
    }
  }
}

TEST(free3d_behind_the_camera_is_reported) {
  Free3DGeometry g = cam(8, 8, 0);
  float ex, ey, ez, sx, sy;
  g.eye(ex, ey, ez);
  CHECK(!g.projectPoint(ex, ey + 1, ez + 5, sx, sy));
  CHECK(g.pixelsAt(ex, ey + 1, ez + 5) == 0);
  CHECK(std::fabs(g.pixelsAt(g.focusX, g.focusY, g.focusZ) - g.focal() / g.distance) < 0.01f);
}

TEST(free3d_pick_finds_every_square_from_every_side) {
  std::vector<int> elev(7 * 5, 0);
  const float pitches[] = {Free3DGeometry::kMinPitch, 0.87f, Free3DGeometry::kMaxPitch};
  for (float yaw = -3.2f; yaw < 3.2f; yaw += 0.37f)
    for (float pitch : pitches) {
      Free3DGeometry g = cam(7, 5, 0, yaw, pitch, 16);
      for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 7; ++x) {
          float sx, sy;
          CHECK(g.project(x + 0.5f, y + 0.5f, 0, sx, sy));
          CHECK(g.pick(sx, sy, elev) == y * 7 + x);
        }
      CHECK(g.pick(-5000, g.viewH * 0.5f, elev) == -1);  // far beside the map
    }
}

TEST(free3d_pick_meets_block_sides_and_hidden_agrees) {
  // A wall along x = 3 on a 7 x 5 map; the camera looks from the west (the wall hides what is east of it).
  std::vector<int> elev(7 * 5, 0);
  for (int y = 0; y < 5; ++y) elev[(size_t)(y * 7 + 3)] = 4;
  Free3DGeometry g = cam(7, 5, 4, -1.5707963f, Free3DGeometry::kMinPitch, 12);
  CHECK(g.quarter() == 3);
  float sx, sy;
  // The square right behind the wall: its middle is covered, and the pixel there shows the wall.
  CHECK(g.hidden(4, 2, elev));
  g.project(4.5f, 2.5f, 0, sx, sy);
  CHECK(g.pick(sx, sy, elev) == 2 * 7 + 3);
  // In front of the wall and on top of it: seen.
  CHECK(!g.hidden(2, 2, elev) && !g.hidden(3, 2, elev));
  g.project(3.5f, 2.5f, 4, sx, sy);
  CHECK(g.pick(sx, sy, elev) == 2 * 7 + 3);
  // The wall's west face, half way up, belongs to the wall.
  g.project(3.0f, 2.5f, 2, sx, sy);
  CHECK(g.pick(sx, sy + 0.5f, elev) == 2 * 7 + 3);
  // From every side: a square that is not hidden picks itself at its middle.
  for (float yaw = 0; yaw < 6.3f; yaw += 0.41f) {
    g.yaw = yaw;
    for (int y = 0; y < 5; ++y)
      for (int x = 0; x < 7; ++x) {
        g.project(x + 0.5f, y + 0.5f, (float)elev[(size_t)(y * 7 + x)], sx, sy);
        if (!g.hidden(x, y, elev)) CHECK(g.pick(sx, sy, elev) == y * 7 + x);
        else CHECK(g.pick(sx, sy, elev) != y * 7 + x);
      }
  }
}

TEST(free3d_shift_into_view_puts_a_point_at_the_margin) {
  Free3DGeometry g = cam(80, 80, 2, 0.4f, 0.7f, 10);
  float dX, dZ, sx, sy;
  g.shiftIntoView(0.5f, 0, -0.5f, 40, 40, dX, dZ);  // near the middle already
  CHECK(dX == 0 && dZ == 0);
  const float far[3][3] = {{15, 1, -12}, {-18, 0, 3}, {2, 0.5f, 19}};
  for (const float* p : far) {
    Free3DGeometry h = g;
    h.shiftIntoView(p[0], p[1], p[2], 40, 30, dX, dZ);
    CHECK(dX != 0 || dZ != 0);
    h.focusX += dX;
    h.focusZ += dZ;
    CHECK(h.projectPoint(p[0], p[1], p[2], sx, sy));
    CHECK(sx > 39.5f && sx < h.viewW - 39.5f && sy > 29.5f && sy < h.viewH - 29.5f);
    h.shiftIntoView(p[0], p[1], p[2], 40, 30, dX, dZ);
    CHECK(std::fabs(dX) < 0.01f && std::fabs(dZ) < 0.01f);
  }
}

TEST(free3d_limits_and_quarters) {
  Free3DGeometry g = cam(10, 6, 0);
  g.pitch = 3;
  g.distance = 500;
  g.focusX = 99;
  g.focusZ = -99;
  g.clamp();
  CHECK(g.pitch == Free3DGeometry::kMaxPitch && g.distance == Free3DGeometry::kMaxDistance);
  CHECK(g.focusX == 0 && g.focusZ == 0);  // a map smaller than the zoomed-out picture: its middle
  g.gridW = 40;
  g.focusX = 99;
  g.clamp();
  CHECK(g.focusX == 20 - 30 * 0.2f);  // otherwise: kept a little inside the outline
  g.pitch = 0;
  g.distance = 0;
  g.clamp();
  CHECK(g.pitch == Free3DGeometry::kMinPitch && g.distance == Free3DGeometry::kMinDistance);
  CHECK(g.pitch > g.fovY * 0.5f);  // every sight line through the screen goes down
  g.yaw = 0.7f;  CHECK(g.quarter() == 0);
  g.yaw = 0.9f;  CHECK(g.quarter() == 1);
  g.yaw = -1.6f; CHECK(g.quarter() == 3);
  g.yaw = 9.5f;  CHECK(g.quarter() == 2);
}
