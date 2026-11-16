// Hexagon maps in the two 3D kinds of view (docs/HEX.md "3D isometric and full 3D"): corners that tile, the 3D space,
// turning in sixths, picking and hiding on six-sided blocks, and the player's MapProj on the fixture games.
#include <algorithm>
#include <cmath>
#include <vector>

#include "battle/map_proj.h"
#include "core/view/free3d.h"
#include "core/view/hex3d.h"
#include "core/view/iso3d.h"
#include "hex_fixture.h"
#include "save_helpers.h"

using namespace ote;

namespace {

Grid gridOf(int flat, int cols, int rows) { return Grid(GridType::Hex, cols, rows, flat ? HexLayout::Flat : HexLayout::Pointy); }

Iso3DGeometry isoOf(const Grid& g, int levels) {
  Iso3DGeometry iso;
  iso.pixels = 48;
  iso.sinPitch = 0.5f;
  iso.stepY = 10.0f / (48.0f * iso.cosPitch());  // 10 px a level: no height lines up exactly with a sight line (borderline cases)
  iso.gridW = g.width();
  iso.gridH = g.height();
  iso.levels = levels;
  iso.headroom = 48;
  iso.hex = HexSpace::make(HexGeo::of(g, 24));
  return iso;
}

Free3DGeometry freeOf(const Grid& g, int levels) {
  Free3DGeometry f;
  f.gridW = g.width();
  f.gridH = g.height();
  f.levels = levels;
  f.hex = HexSpace::make(HexGeo::of(g, 24));
  return f;
}

}  // namespace

TEST(hex3d_corners_of_neighbours_coincide_and_edge_i_faces_direction_i) {
  for (int flat = 0; flat < 2; ++flat) {
    Grid g = gridOf(flat, 7, 6);
    HexGeo geo = HexGeo::of(g, 24);
    for (Cell c = 0; c < g.cellCount(); ++c) {
      float a[6][2];
      geo.corners(g.x(c), g.y(c), a);
      // the box's four sides are touched, as the contract's picture says
      float lo[2] = {1e9f, 1e9f}, hi[2] = {-1e9f, -1e9f};
      int bx, by;
      geo.box(g.x(c), g.y(c), bx, by);
      for (int i = 0; i < 6; ++i)
        for (int k = 0; k < 2; ++k) {
          lo[k] = std::min(lo[k], a[i][k]);
          hi[k] = std::max(hi[k], a[i][k]);
        }
      CHECK(lo[0] == bx && lo[1] == by && hi[0] == bx + geo.boxW() && hi[1] == by + geo.boxH());
      for (int d = 0; d < 6; ++d) {
        Cell n = g.neighbor(c, d);
        if (n == kNoCell) continue;
        float b[6][2];
        geo.corners(g.x(n), g.y(n), b);
        // edge d of c (corners d, d+1) is edge d + 3 of the neighbour, walked the other way
        const int e = (d + 3) % 6;
        CHECK(a[d][0] == b[(e + 1) % 6][0] && a[d][1] == b[(e + 1) % 6][1]);
        CHECK(a[(d + 1) % 6][0] == b[e][0] && a[(d + 1) % 6][1] == b[e][1]);
      }
    }
  }
}

TEST(hex3d_space_is_the_top_down_picture_in_squares) {
  for (int flat = 0; flat < 2; ++flat) {
    Grid g = gridOf(flat, 7, 6);
    HexSpace hs = HexSpace::make(HexGeo::of(g, 24));
    CHECK(std::fabs(hs.width() - hs.geo.mapW() / 24.0f) < 1e-5f);
    for (Cell c = 0; c < g.cellCount(); ++c) {
      float X, Z;
      hs.middle(g.x(c), g.y(c), X, Z);
      CHECK_EQ(hs.cellAt(X, Z), (int)c);  // the nearest middle is itself
      // a point a little towards a corner still belongs to the hexagon
      float cx, cz;
      hs.corner(g.x(c), g.y(c), 1, cx, cz);
      CHECK_EQ(hs.cellAt(X + (cx - X) * 0.7f, Z + (cz - Z) * 0.7f), (int)c);
    }
    CHECK_EQ(hs.cellAt(-hs.width(), 0), -1);  // far off the map
  }
}

TEST(hex3d_isometric_turns_in_sixths_and_turn_0_is_the_squashed_top_down_map) {
  CHECK(std::fabs(Iso3DGeometry::yawOfHexTurn(6) - 6.2831853f) < 1e-4f);
  CHECK_EQ(Iso3DGeometry::yawOfHexTurn(0), 0.0f);
  Grid g = gridOf(0, 6, 5);
  Iso3DGeometry iso = isoOf(g, 2);
  iso.yaw = Iso3DGeometry::yawOfHexTurn(0);
  // at turn 0 two cells are apart by the top-down distance times tileWidth / tileSize horizontally and half of it down
  float ax, ay, bx, by, fa, fb;
  iso.hex.middle(1, 1, fa, fb);
  iso.projectPoint(fa, 0, fb, ax, ay);
  iso.hex.middle(3, 2, fa, fb);
  iso.projectPoint(fa, 0, fb, bx, by);
  HexGeo geo = iso.hex.geo;
  int m1x, m1y, m2x, m2y;
  geo.middle(1, 1, m1x, m1y);
  geo.middle(3, 2, m2x, m2y);
  CHECK(std::fabs((bx - ax) - (m2x - m1x) * 2.0f) < 1e-3f);
  CHECK(std::fabs((by - ay) - (m2y - m1y) * 1.0f) < 1e-3f);
  // a whole circle of six turns comes back
  iso.yaw = Iso3DGeometry::yawOfHexTurn(6);
  float cx, cy;
  iso.hex.middle(3, 2, fa, fb);
  iso.projectPoint(fa, 0, fb, cx, cy);
  CHECK(std::fabs(cx - bx) < 1e-2f && std::fabs(cy - by) < 1e-2f);
}

TEST(hex3d_isometric_pick_finds_the_block_at_every_turn_and_hidden_agrees) {
  for (int flat = 0; flat < 2; ++flat) {
    Grid g = gridOf(flat, 8, 7);
    std::vector<int> elev((size_t)g.cellCount(), 0);
    const Cell hi = g.at(4, 3);
    elev[(size_t)hi] = 3;
    elev[(size_t)g.neighbor(hi, 2)] = 1;
    Iso3DGeometry iso = isoOf(g, 3);
    for (int turn = 0; turn < 6; ++turn) {
      iso.yaw = Iso3DGeometry::yawOfHexTurn((float)turn);
      int hiddenCount = 0;
      for (Cell c = 0; c < g.cellCount(); ++c) {
        float X, Z, wx, wy;
        iso.hex.middle(g.x(c), g.y(c), X, Z);
        iso.projectPoint(X, elev[(size_t)c] * iso.stepY, Z, wx, wy);
        const int got = iso.pick(wx, wy, elev);
        if (!iso.hidden(g.x(c), g.y(c), elev)) CHECK_EQ(got, (int)c);
        else {
          ++hiddenCount;
          CHECK(got != (int)c && got >= 0);  // something nearer covers it
        }
      }
      // the tall block hides at least one hexagon behind it from every side but never itself
      CHECK(hiddenCount >= 1);
      CHECK(!iso.hidden(g.x(hi), g.y(hi), elev));
    }
    // off the picture: nothing
    CHECK_EQ(iso.pick(-50, -50, elev), -1);
  }
}

TEST(hex3d_free_camera_pick_finds_the_block_and_hidden_agrees) {
  for (int flat = 0; flat < 2; ++flat) {
    Grid g = gridOf(flat, 8, 7);
    std::vector<int> elev((size_t)g.cellCount(), 0);
    const Cell hi = g.at(4, 3);
    elev[(size_t)hi] = 3;
    Free3DGeometry f = freeOf(g, 3);
    f.stepY = 0.5f;
    for (int k = 0; k < 4; ++k) {
      f.yaw = k * 1.5707963f + 0.3f;
      int hiddenCount = 0;
      for (Cell c = 0; c < g.cellCount(); ++c) {
        float X, Z, sx, sy;
        f.hex.middle(g.x(c), g.y(c), X, Z);
        f.focusX = X;
        f.focusZ = Z;  // looking right at it
        CHECK(f.projectPoint(X, elev[(size_t)c] * f.stepY, Z, sx, sy));
        const int got = f.pick(sx, sy, elev);
        if (!f.hidden(g.x(c), g.y(c), elev)) CHECK_EQ(got, (int)c);
        else {
          ++hiddenCount;
          CHECK(got != (int)c && got >= 0);
        }
      }
      CHECK(!f.hidden(g.x(hi), g.y(hi), elev));
      (void)hiddenCount;
    }
    // From the south on a low camera, the neighbour straight behind the tall block (the one furthest north) is covered.
    f.yaw = 0;
    f.pitch = 0.5f;
    f.distance = 8;
    f.focusX = f.focusZ = 0;
    Cell behind = kNoCell;
    float bestZ = 1e9f;
    for (int d = 0; d < 6; ++d) {
      Cell n = g.neighbor(hi, d);
      float X, Z;
      f.hex.middle(g.x(n), g.y(n), X, Z);
      if (Z < bestZ - 1e-3f) { bestZ = Z; behind = n; }
    }
    CHECK(f.hidden(g.x(behind), g.y(behind), elev));
    CHECK_EQ(f.pick(-5000, -5000, elev), -1);
  }
}

namespace {

StyleDef style3d(StyleView v, int tileWidth = 0, int step = -1, int unitWidth = 0) {
  StyleDef st;
  st.view = v;
  st.iso.tileWidth = tileWidth;
  st.iso.heightStep = step;
  st.iso.unitWidth = unitWidth;
  return st;
}

}  // namespace

TEST(hex3d_map_proj_on_the_fixture_games) {
  for (int flat = 0; flat < 2; ++flat) {
    savetest::Rig rig;
    CHECK(rig.load(ote_test::sourceDir() + (flat ? "/tests/fixtures/hex-flat" : "/tests/fixtures/hex-pointy"), "skirmish"));
    rig.start();
    const Grid& g = rig.battle.grid;
    const Cell hi = g.at(5, 4);
    rig.battle.cells[(size_t)hi].height = 3;
    rig.battle.cells[(size_t)g.neighbor(hi, 0)].height = 1;
    for (int kind = 0; kind < 2; ++kind) {
      StyleDef st = style3d(kind ? StyleView::Full3D : StyleView::Isometric3D, 0, 10);  // 10 px a level (the free camera ignores it)
      MapProj proj;
      proj.init(rig.battle, &st, 24);
      CHECK(proj.hex() && proj.view3d() && proj.raised() && !proj.iso());
      CHECK(kind ? proj.free3d() : proj.iso3d());
      CHECK(proj.hexSpace().on);
      CHECK_EQ(proj.turnCount(), 6);
      CHECK(std::fabs(proj.spaceW() - proj.hexGeo().mapW() / 24.0f) < 1e-5f);
      if (!kind) {
        CHECK_EQ(proj.unitW(), 24);  // the 2D isometric defaults: tileWidth 2 x tileSize, unitWidth tileWidth / 2 (heightStep tileWidth / 4 = 12 unless given)
        CHECK_EQ(proj.geo3().pixels, 48.0f);
        CHECK_EQ(proj.geo3().levels, 3);
        CHECK(std::fabs(proj.geo3().stepY * proj.geo3().pixels * proj.geo3().cosPitch() - 10.0f) < 1e-3f);  // heightStep 10: a level is 10 px
        proj.setTurn(2);
        CHECK(std::fabs(proj.yaw() - 2.0943951f) < 1e-4f);  // 120 degrees
        proj.setTurn(0);
      } else {
        proj.setViewport(352, 259);
      }
      for (Cell c = 0; c < g.cellCount(); ++c) {
        const int x = g.x(c), y = g.y(c);
        float gx, gy, fx, fy;
        proj.cellMid(x, y, gx, gy);
        cellCentre(g, 24, c, fx, fy);
        CHECK(std::fabs(gx * 24 - fx) < 1e-3f && std::fabs(gy * 24 - fy) < 1e-3f);
        CHECK_EQ(proj.elevationAt(gx, gy), (float)proj.elevation(x, y));
        int bx, by, bw, bh;
        proj.cellBox(x, y, bx, by, bw, bh);
        CHECK(bw > 10 && bh > 5);
        float wx, wy;
        proj.toWorld(fx, fy, wx, wy);  // a unit on the cell: the middle of its top face
        CHECK(wx >= bx - 1 && wx <= bx + bw + 1 && wy >= by - 1 && wy <= by + bh + 1);
        // pointing at the top face's middle gets the cell unless something nearer covers it (not asked next to the tall
        // block: from some sides the sight line then runs exactly along the border of two hexagons, a tie either way)
        if (!proj.hidden3(x, y) && g.distance(c, hi) > 2) CHECK_EQ(proj.pick((int)std::lround(wx), (int)std::lround(wy)), c);
      }
      // halfway between the 3 and the 1 hexagon the ground eases to 2
      float ax, ay, bx2, by2;
      cellCentre(g, 24, hi, ax, ay);
      cellCentre(g, 24, g.neighbor(hi, 0), bx2, by2);
      CHECK(std::fabs(proj.elevationAt((ax + bx2) * 0.5f / 24, (ay + by2) * 0.5f / 24) - 2.0f) < 0.01f);
    }
    // styles: tileWidth, heightStep and unitWidth of the 3D isometric kind follow the 2D rules
    StyleDef st2 = style3d(StyleView::Isometric3D, 36, 6, 20);
    MapProj p2;
    p2.init(rig.battle, &st2, 24);
    CHECK(p2.geo3().pixels == 36.0f && p2.unitW() == 20);
    CHECK(std::fabs(p2.geo3().stepY * p2.geo3().pixels * p2.geo3().cosPitch() - 6.0f) < 1e-3f);
  }
}

TEST(hex3d_square_maps_keep_their_quarters) {
  savetest::Rig rig;
  CHECK(rig.load(ote_test::sourceDir() + "/tests/fixtures/hex-pointy", "skirmish"));
  rig.start();
  StyleDef st = style3d(StyleView::Isometric3D);
  MapProj hex;
  hex.init(rig.battle, &st, 24);
  CHECK_EQ(hex.turnCount(), 6);
  hex.setTurn(1);
  CHECK(std::fabs(hex.yaw() - 1.0471976f) < 1e-5f);  // sixths: 60 degrees
  // (the square turn rule, (n + 0.5) quarters, is the one tests/test_iso3d.cpp covers)
  CHECK(std::fabs(Iso3DGeometry::yawOfTurn(1) - 2.3561945f) < 1e-5f);
}
