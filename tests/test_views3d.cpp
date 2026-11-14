// Geometry of the "isometric-3d" kind of view (docs/VIEWS.md).
#include <cmath>

#include "core/view/iso.h"
#include "core/view/iso3d.h"
#include "test.h"

using namespace ote;

namespace {
Iso3DGeometry geo3(int w, int h, int levels, float turn = 0) {
  Iso3DGeometry g;
  g.setFromDiamond(48, 24, 12);
  g.gridW = w;
  g.gridH = h;
  g.levels = levels;
  g.yaw = Iso3DGeometry::yawOfTurn(turn);
  return g;
}
}  // namespace

TEST(views3d_turn_zero_is_the_2d_isometric_picture) {
  IsoGeometry flat;
  flat.gridW = 6;
  flat.gridH = 4;
  flat.levels = 3;
  Iso3DGeometry g = geo3(6, 4, 3);
  // The same differences between any two spots (the two boxes have different origins).
  float ax, ay, bx, by, cx, cy, dx, dy;
  flat.project(0, 0, 0, ax, ay);
  g.project(0, 0, 0, bx, by);
  const float spots[4][3] = {{1, 0, 0}, {0, 1, 0}, {3.5f, 2.5f, 2}, {6, 4, 3}};
  for (const float* s : spots) {
    flat.project(s[0], s[1], s[2], cx, cy);
    g.project(s[0], s[1], s[2], dx, dy);
    CHECK(std::fabs((cx - ax) - (dx - bx)) < 0.01f);
    CHECK(std::fabs((cy - ay) - (dy - by)) < 0.01f);
  }
}

TEST(views3d_everything_stays_inside_the_world_box_at_every_turn) {
  for (float turn = 0; turn < 4; turn += 0.25f) {
    Iso3DGeometry g = geo3(7, 3, 4, turn);
    for (int corner = 0; corner < 8; ++corner) {
      float wx, wy;
      g.project(corner & 1 ? 7.0f : 0.0f, corner & 2 ? 3.0f : 0.0f, corner & 4 ? 4.0f : -1.0f, wx, wy);
      CHECK(wx >= 0 && wy >= 0 && wx <= g.size() && wy <= g.size());
    }
  }
}

TEST(views3d_pick_finds_every_square_at_every_quarter_turn) {
  // A flat map: the middle of every top face picks its own square.
  std::vector<int> elev(5 * 4, 0);
  for (int turn = 0; turn < 4; ++turn) {
    Iso3DGeometry g = geo3(5, 4, 0, (float)turn);
    for (int y = 0; y < 4; ++y)
      for (int x = 0; x < 5; ++x) {
        float wx, wy, gx, gy;
        g.project(x + 0.5f, y + 0.5f, 0, wx, wy);
        CHECK_EQ(g.pick(wx, wy, elev), y * 5 + x);
        g.unprojectAt(wx, wy, 0, gx, gy);
        CHECK(std::fabs(gx - (x + 0.5f)) < 0.01f && std::fabs(gy - (y + 0.5f)) < 0.01f);
      }
    CHECK_EQ(g.pick(1, 1, elev), -1);  // a corner of the box: off the map
  }
}

TEST(views3d_pick_and_hidden_respect_raised_blocks) {
  // 3 x 3, the middle square is a tower of 6 levels.
  std::vector<int> elev(9, 0);
  elev[4] = 6;
  for (int turn = 0; turn < 4; ++turn) {
    Iso3DGeometry g = geo3(3, 3, 6, (float)turn);
    float wx, wy;
    g.project(1.5f, 1.5f, 6, wx, wy);
    CHECK_EQ(g.pick(wx, wy, elev), 4);  // its top
    g.project(1.5f, 1.5f, 3, wx, wy);   // halfway up the tower's middle axis: its near faces
    CHECK_EQ(g.pick(wx, wy, elev), 4);
    // The square straight behind the tower (away from the camera) is covered; the one in front is not.
    int bx = turn == 0 || turn == 1 ? 0 : 2, by = turn == 0 || turn == 3 ? 0 : 2;  // the camera stands over the opposite corner
    CHECK(g.hidden(bx, by, elev));
    CHECK(!g.hidden(2 - bx, 2 - by, elev));
    CHECK(!g.hidden(1, 1, elev));
    // Where the hidden square's middle would be on screen, the tower is what is picked.
    g.project(bx + 0.5f, by + 0.5f, 0, wx, wy);
    CHECK_EQ(g.pick(wx, wy, elev), 4);
  }
}
