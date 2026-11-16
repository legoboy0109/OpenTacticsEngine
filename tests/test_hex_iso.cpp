// Hexagon maps in the 2D isometric view (docs/HEX.md "2D isometric"): the rounding rule, that neighbouring squashed
// hexagons share edges exactly, picking and hiding with heights, and the player's MapProj on the fixture games.
#include <algorithm>
#include <cmath>
#include <vector>

#include "battle/map_proj.h"
#include "core/view/hex_iso.h"
#include "hex_fixture.h"
#include "save_helpers.h"

using namespace ote;

namespace {

HexIsoGeometry mk(int flat, int tile, int tileWidth, int cols, int rows, int step = 12) {
  Grid g(GridType::Hex, cols, rows, flat ? HexLayout::Flat : HexLayout::Pointy);
  return HexIsoGeometry::make(g, tile, tileWidth, step);
}

Grid gridOf(int flat, int cols, int rows) { return Grid(GridType::Hex, cols, rows, flat ? HexLayout::Flat : HexLayout::Pointy); }

}  // namespace

TEST(hex_iso_box_sizes_follow_the_rounding_rule) {
  // pointy: tileWidth wide, the multiple of 4 nearest to L * tileWidth / (2 * tile) tall
  HexIsoGeometry p = mk(0, 24, 48, 12, 9);
  CHECK(p.boxW() == 48 && p.boxH() == 28);
  CHECK_EQ(p.sq.quarter() * 3, 21);  // rows are 21 apart, as top-down
  // flat: the multiple of 4 nearest to L * tileWidth / tile wide, tileWidth / 2 tall
  HexIsoGeometry f = mk(1, 24, 48, 12, 9);
  CHECK(f.boxW() == 56 && f.boxH() == 24);
  CHECK(mk(0, 16, 32, 5, 5).boxW() == 32 && mk(0, 16, 32, 5, 5).boxH() == 20);
  CHECK(mk(1, 16, 32, 5, 5).boxW() == 40 && mk(1, 16, 32, 5, 5).boxH() == 16);
  // another tileWidth: 36 over a tile of 24 is a squash of 1.5 / 0.75
  CHECK(mk(0, 24, 36, 5, 5).boxW() == 36 && mk(0, 24, 36, 5, 5).boxH() == 20);  // 21 -> 20
  // 42 would be 44, but 18 x 44 leaves a seam (9 rows, rise 11): the nearer multiple of 4 that tiles is 40
  CHECK(mk(1, 24, 36, 5, 5).boxW() == 40 && mk(1, 24, 36, 5, 5).boxH() == 18);
  // an odd half of tileWidth is rounded up to even on a flat map (the half shift must be whole pixels)
  CHECK_EQ(mk(1, 24, 50, 5, 5).boxH(), 26);

  // world size: the lattice's map size plus room for the heights and the headroom above
  p.levels = 2;
  CHECK_EQ(p.worldW(), 12 * 48 + 24);
  CHECK_EQ(p.worldH(), 8 * 21 + 28 + 2 * 12 + 12 + p.headroom);
  CHECK_EQ(p.headroom, 48);
}

TEST(hex_iso_a_cells_middle_is_its_top_down_middle_scaled) {
  for (int flat = 0; flat < 2; ++flat)
    for (int tile = 16; tile <= 32; tile += 8)
      for (int wmul = 2; wmul <= 3; ++wmul) {
        HexIsoGeometry g = mk(flat, tile, tile * wmul, 7, 6);
        g.levels = 3;
        for (int y = 0; y < 6; ++y)
          for (int x = 0; x < 7; ++x) {
            int tx, ty, sx, sy;
            g.td.middle(x, y, tx, ty);
            g.sq.middle(x, y, sx, sy);
            for (int e = 0; e <= 3; ++e) {
              float wx, wy;
              g.project((float)tx, (float)ty, (float)e, wx, wy);
              int bx, by;
              g.topBox(x, y, e, bx, by);
              CHECK(std::fabs(wx - (bx + g.boxW() / 2)) < 0.01f);
              CHECK(std::fabs(wy - (by + g.boxH() / 2)) < 0.01f);
              float fx, fy;
              g.unproject(wx, wy, (float)e, fx, fy);
              CHECK(std::fabs(fx - tx) < 0.01f && std::fabs(fy - ty) < 0.01f);
            }
            (void)sx;
            (void)sy;
          }
      }
}

TEST(hex_iso_neighbouring_hexagons_share_edges_exactly) {
  for (int flat = 0; flat < 2; ++flat)
    for (int tile : {16, 24, 32})
      for (int tw = 8; tw <= 130; tw += 2) {
        if (tw > 40 && tw % 8 != 0 && tw != tile * 3) continue;  // every width up to 40, then a spread of larger ones
        const int cols = 6, rows = 6;
        HexIsoGeometry g = mk(flat, tile, tw, cols, rows);
        Grid grid = gridOf(flat, cols, rows);
        const int W = g.sq.mapW(), H = g.sq.mapH();
        std::vector<int> owner((size_t)(W * H), -1);
        int overlaps = 0;
        for (int y = 0; y < rows; ++y)
          for (int x = 0; x < cols; ++x) {
            int bx, by;
            g.sq.box(x, y, bx, by);
            for (int row = 0; row < g.boxH(); ++row) {
              int l, r;
              g.sq.rowSpan(row, l, r);
              CHECK(l >= 0 && r <= g.boxW() && l < r);
              for (int px = bx + l; px < bx + r; ++px) {
                int& o = owner[(size_t)((by + row) * W + px)];
                if (o >= 0) ++overlaps;
                o = y * cols + x;
              }
            }
          }
        CHECK_EQ(overlaps, 0);  // no pixel in two hexagons
        // No gap in the middle of the map (the notches at its rim are outside every hexagon).
        int gaps = 0;
        for (int y = g.boxH(); y < H - g.boxH(); ++y)
          for (int x = g.boxW(); x < W - g.boxW(); ++x)
            if (owner[(size_t)(y * W + x)] < 0) ++gaps;
        CHECK_EQ(gaps, 0);
        // Every pixel's right and lower neighbour belongs to the same hexagon or to a lattice neighbour of it (so
        // hexagons touch only along the six shared edges), and every neighbouring pair does touch.
        std::vector<unsigned> touches((size_t)(cols * rows), 0);
        int strangers = 0;
        for (int y = 0; y + 1 < H; ++y)
          for (int x = 0; x + 1 < W; ++x) {
            int a = owner[(size_t)(y * W + x)];
            for (int other : {owner[(size_t)(y * W + x + 1)], owner[(size_t)((y + 1) * W + x)]}) {
              if (a < 0 || other < 0 || a == other) continue;
              int dirA = grid.directionTo((Cell)a, (Cell)other), dirB = grid.directionTo((Cell)other, (Cell)a);
              if (dirA < 0 || dirB < 0) ++strangers;
              else {
                touches[(size_t)a] |= 1u << dirA;
                touches[(size_t)other] |= 1u << dirB;
              }
            }
          }
        CHECK_EQ(strangers, 0);
        for (int y = 0; y < rows; ++y)
          for (int x = 0; x < cols; ++x)
            for (int d = 0; d < 6; ++d)
              if (grid.neighbor(grid.at(x, y), d) != kNoCell) CHECK((touches[(size_t)(y * cols + x)] >> d) & 1u);
      }
}

TEST(hex_iso_side_faces_lie_under_the_lower_edges) {
  HexIsoGeometry p = mk(0, 24, 48, 3, 3), f = mk(1, 24, 48, 3, 3);
  CHECK_EQ(p.visibleFaces(), (1u << 2) | (1u << 3));
  CHECK_EQ(f.visibleFaces(), (1u << 2) | (1u << 3) | (1u << 4));
  int l, r;
  p.faceSpan(3, l, r);
  CHECK(l == 0 && r == 24);
  p.faceSpan(2, l, r);
  CHECK(l == 24 && r == 48);
  f.faceSpan(4, l, r);
  CHECK(l == 0 && r == 14);
  f.faceSpan(3, l, r);
  CHECK(l == 14 && r == 42);
  f.faceSpan(2, l, r);
  CHECK(l == 42 && r == 56);
  // Back to front: a lower row has the bigger key; on a flat map an odd column (half a hexagon lower) is later.
  CHECK(p.sortKey(0, 1) > p.sortKey(5, 0));
  CHECK(f.sortKey(1, 0) > f.sortKey(0, 0) && f.sortKey(0, 1) > f.sortKey(1, 0));
}

TEST(hex_iso_picking_follows_the_nearest_hexagon_and_the_heights) {
  for (int flat = 0; flat < 2; ++flat) {
    const int cols = 7, rows = 7;
    HexIsoGeometry g = mk(flat, 24, 48, cols, rows);
    Grid grid = gridOf(flat, cols, rows);
    std::vector<int> elev((size_t)(cols * rows), 0);
    // On a flat map the middle of every top face, and points a third of the way to a neighbour, pick the cell.
    for (Cell c = 0; c < grid.cellCount(); ++c) {
      int bx, by;
      g.topBox(grid.x(c), grid.y(c), 0, bx, by);
      float mx = bx + g.boxW() * 0.5f, my = by + g.boxH() * 0.5f;
      CHECK_EQ(g.pick(mx, my, elev), (int)c);
      for (int d = 0; d < 6; ++d) {
        Cell nb = grid.neighbor(c, d);
        if (nb == kNoCell) continue;
        int nx, ny;
        g.topBox(grid.x(nb), grid.y(nb), 0, nx, ny);
        float px = nx + g.boxW() * 0.5f, py = ny + g.boxH() * 0.5f;
        CHECK_EQ(g.pick(mx + (px - mx) * 0.3f, my + (py - my) * 0.3f, elev), (int)c);
        CHECK_EQ(g.pick(mx + (px - mx) * 0.7f, my + (py - my) * 0.7f, elev), (int)nb);
      }
    }
    CHECK_EQ(g.pick(-5, 10, elev), -1);
    CHECK_EQ(g.pick(10, (float)g.worldH() + 40, elev), -1);

    // A tall block in the middle: its top face, its side faces and the ground it covers all pick it. `behind` is a
    // hexagon the block's lifted top face covers: flat, straight up; pointy, two rows up (the one in the row above
    // sits exactly on the block's vertical edge, so which it picks there is a toss-up).
    const int x0 = 3, y0 = 3;
    Cell mid = grid.at(x0, y0), behind = grid.at(x0, y0 - 2);
    if (flat) behind = grid.neighbor(mid, 0);
    elev[(size_t)mid] = 4;
    g.levels = 4;
    int bx, by;
    g.topBox(x0, y0, 4, bx, by);
    CHECK_EQ(g.pick(bx + g.boxW() * 0.5f, by + g.boxH() * 0.5f, elev), (int)mid);
    g.topBox(x0, y0, 0, bx, by);
    CHECK_EQ(g.pick(bx + g.boxW() * 0.5f, by + g.boxH() - 6, elev), (int)mid);  // low on its side face
    g.topBox(grid.x(behind), grid.y(behind), 0, bx, by);
    CHECK_EQ(g.pick(bx + g.boxW() * 0.5f, by + g.boxH() * 0.5f, elev), (int)mid);  // the block is nearer
    CHECK(g.hidden(grid.x(behind), grid.y(behind), elev));
    CHECK(!g.hidden(x0, y0, elev));
    CHECK(!g.hidden(0, 0, elev));
    // A cell in front of the block is never covered by it.
    CHECK(!g.hidden(x0, y0 + 2, elev));
    g.topBox(x0, y0 + 2, 0, bx, by);
    CHECK_EQ(g.pick(bx + g.boxW() * 0.5f, by + g.boxH() * 0.5f, elev), (int)grid.at(x0, y0 + 2));
    // A one-level block does not reach two rows back.
    elev[(size_t)mid] = 1;
    g.levels = 1;
    g.topBox(x0, y0 - 2, 0, bx, by);
    CHECK_EQ(g.pick(bx + g.boxW() * 0.5f, by + g.boxH() * 0.5f, elev), (int)grid.at(x0, y0 - 2));
    CHECK(!g.hidden(x0, y0 - 2, elev));
  }
}

namespace {

StyleDef isoStyle(int tileWidth = 0, int step = -1, int unitWidth = 0) {
  StyleDef st;
  st.view = StyleView::Isometric;
  st.iso.tileWidth = tileWidth;
  st.iso.heightStep = step;
  st.iso.unitWidth = unitWidth;
  return st;
}

}  // namespace

TEST(hex_iso_map_proj_on_the_fixture_games) {
  for (int flat = 0; flat < 2; ++flat) {
    savetest::Rig rig;
    CHECK(rig.load(ote_test::sourceDir() + (flat ? "/tests/fixtures/hex-flat" : "/tests/fixtures/hex-pointy"), "skirmish"));
    rig.start();
    const Grid& g = rig.battle.grid;
    // heights: one hexagon at 3, its neighbour at 1, the rest 0
    const Cell hi = g.at(5, 4);
    rig.battle.cells[(size_t)hi].height = 3;
    rig.battle.cells[(size_t)g.neighbor(hi, 0)].height = 1;
    StyleDef st = isoStyle();
    MapProj proj;
    proj.init(rig.battle, &st, 24);
    CHECK(proj.hex() && proj.iso() && proj.raised() && !proj.view3d());
    CHECK_EQ(proj.hexIso().tileW, 48);  // default 2 x tileSize
    CHECK_EQ(proj.hexIso().step, 12);   // default tileWidth / 4
    CHECK_EQ(proj.unitW(), 24);         // default tileWidth / 2
    CHECK_EQ(proj.hexIso().levels, 3);
    CHECK_EQ(proj.worldW(), proj.hexIso().worldW());
    CHECK_EQ(proj.worldH(), proj.hexIso().worldH());
    CHECK_EQ(proj.worldW(), flat ? 11 * 42 + 56 : 12 * 48 + 24);
    CHECK_EQ((int)proj.hexDrawOrder().size(), g.cellCount());
    // The back-to-front order never goes backwards.
    int last = -1;
    for (Cell c : proj.hexDrawOrder()) {
      int k = proj.hexIso().sortKey(g.x(c), g.y(c));
      CHECK(k >= last);
      last = k;
    }
    for (Cell c = 0; c < g.cellCount(); ++c) {
      int bx, by, bw, bh;
      proj.cellBox(g.x(c), g.y(c), bx, by, bw, bh);
      CHECK(bw == (flat ? 56 : 48) && bh == (flat ? 24 : 28));
      // a unit standing on the cell is where the top face's middle is, whatever the height
      float fx, fy, wx, wy;
      cellCentre(g, 24, c, fx, fy);
      proj.toWorld(fx, fy, wx, wy);
      CHECK(std::fabs(wx - (bx + bw / 2)) < 0.01f && std::fabs(wy - (by + bh / 2)) < 0.01f);
      CHECK_EQ(proj.hexElevationAt(fx, fy), (float)proj.elevation(g.x(c), g.y(c)));
      // the pointer on the top face's middle gets the cell unless something nearer covers it
      if (!proj.hexIso().hidden(g.x(c), g.y(c), proj.elevations())) CHECK_EQ(proj.pick(bx + bw / 2, by + bh / 2), c);
    }
    // between the 3 and the 1 hexagon the ground eases: halfway is 2
    float ax, ay, bx2, by2;
    cellCentre(g, 24, hi, ax, ay);
    cellCentre(g, 24, g.neighbor(hi, 0), bx2, by2);
    CHECK(std::fabs(proj.hexElevationAt((ax + bx2) * 0.5f, (ay + by2) * 0.5f) - 2.0f) < 0.01f);
    CHECK_EQ(proj.pick(-3, 5), kNoCell);

    // styles: tileWidth, heightStep and unitWidth are honoured
    StyleDef st2 = isoStyle(36, 6, 20);
    MapProj p2;
    p2.init(rig.battle, &st2, 24);
    CHECK(p2.hexIso().tileW == 36 && p2.hexIso().step == 6 && p2.unitW() == 20);
    // a 3D style on a hexagon game is 3D (tests/test_hex3d.cpp), not the 2D isometric view
    StyleDef st3;
    st3.view = StyleView::Isometric3D;
    MapProj p3;
    p3.init(rig.battle, &st3, 24);
    CHECK(p3.hex() && p3.iso3d() && p3.view3d() && !p3.iso());
  }
}
