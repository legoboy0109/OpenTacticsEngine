// Hexagon maps in the top-down picture (docs/HEX.md "Top-down picture"): box positions, the map's size in
// pixels, which hexagon a point picks, that the drawn hexagons tile exactly, and the player's MapProj on a
// real fixture game.
#include <cmath>
#include <vector>

#include "battle/map_proj.h"
#include "core/view/hex_geo.h"
#include "hex_fixture.h"
#include "save_helpers.h"

using namespace ote;

namespace {

HexGeo geo(int flat, int tile, int w, int h) {
  Grid g(GridType::Hex, w, h, flat ? HexLayout::Flat : HexLayout::Pointy);
  return HexGeo::of(g, tile);
}

}  // namespace

TEST(hex_long_side_is_the_multiple_of_four_nearest_to_1_1547_tile) {
  CHECK_EQ(HexGeo::longSide(24), 28);
  CHECK_EQ(HexGeo::longSide(16), 20);
  CHECK_EQ(HexGeo::longSide(32), 36);
  CHECK_EQ(HexGeo::longSide(8), 8);
  CHECK_EQ(HexGeo::longSide(48), 56);
}

TEST(hex_boxes_and_map_size_follow_the_contract) {
  // pointy T=24, L=28: box left x*24 (+12 on odd rows), top y*21; map 12*24+12 by 8*21+28
  HexGeo p = geo(0, 24, 12, 9);
  int bx, by;
  p.box(0, 0, bx, by);
  CHECK(bx == 0 && by == 0);
  p.box(3, 4, bx, by);
  CHECK(bx == 72 && by == 84);
  p.box(3, 5, bx, by);
  CHECK(bx == 84 && by == 105);
  CHECK(p.boxW() == 24 && p.boxH() == 28);
  CHECK_EQ(p.mapW(), 12 * 24 + 12);
  CHECK_EQ(p.mapH(), 8 * 21 + 28);
  p.middle(3, 5, bx, by);
  CHECK(bx == 96 && by == 119);

  // flat T=24, L=28: box left x*21, top y*24 (+12 on odd columns); map 11*21+28 by 9*24+12
  HexGeo f = geo(1, 24, 12, 9);
  f.box(3, 4, bx, by);
  CHECK(bx == 63 && by == 108);
  f.box(2, 4, bx, by);
  CHECK(bx == 42 && by == 96);
  CHECK(f.boxW() == 28 && f.boxH() == 24);
  CHECK_EQ(f.mapW(), 11 * 21 + 28);
  CHECK_EQ(f.mapH(), 9 * 24 + 12);

  // T=16, L=20: step 15
  HexGeo p16 = geo(0, 16, 10, 6);
  p16.box(2, 3, bx, by);
  CHECK(bx == 2 * 16 + 8 && by == 45);
  CHECK_EQ(p16.mapW(), 10 * 16 + 8);
  CHECK_EQ(p16.mapH(), 5 * 15 + 20);
  HexGeo f16 = geo(1, 16, 10, 6);
  f16.box(3, 2, bx, by);
  CHECK(bx == 45 && by == 2 * 16 + 8);
  CHECK_EQ(f16.mapW(), 9 * 15 + 20);
  CHECK_EQ(f16.mapH(), 6 * 16 + 8);
}

TEST(hex_structure_boxes_cover_the_seven_hexagons) {
  for (int flat = 0; flat < 2; ++flat)
    for (int tile = 16; tile <= 32; tile += 8) {
      HexGeo g = geo(flat, tile, 9, 9);
      const int L = g.L;
      int bx, by, bw, bh;
      g.structureBox(4, 4, 0, bx, by, bw, bh);
      int cbx, cby;
      g.box(4, 4, cbx, cby);
      CHECK(bx == cbx && by == cby && bw == g.boxW() && bh == g.boxH());
      g.structureBox(4, 4, 1, bx, by, bw, bh);
      CHECK_EQ(bw, flat ? 3 * L / 2 + L : 3 * tile);
      CHECK_EQ(bh, flat ? 3 * tile : 3 * L / 2 + L);
      // The box is exactly the bounding box of the seven hexagon boxes.
      Grid grid(GridType::Hex, 9, 9, flat ? HexLayout::Flat : HexLayout::Pointy);
      std::vector<Cell> ring;
      CHECK(grid.ringCells(grid.at(4, 4), 1, ring));
      int lo[2] = {1 << 30, 1 << 30}, hi[2] = {-(1 << 30), -(1 << 30)};
      for (Cell c : ring) {
        g.box(grid.x(c), grid.y(c), cbx, cby);
        lo[0] = std::min(lo[0], cbx);
        lo[1] = std::min(lo[1], cby);
        hi[0] = std::max(hi[0], cbx + g.boxW());
        hi[1] = std::max(hi[1], cby + g.boxH());
      }
      CHECK(bx == lo[0] && by == lo[1] && bx + bw == hi[0] && by + bh == hi[1]);
      // Every parity of the middle gives the same size.
      for (int y = 3; y <= 4; ++y)
        for (int x = 3; x <= 4; ++x) {
          int ax, ay, aw, ah;
          g.structureBox(x, y, 1, ax, ay, aw, ah);
          CHECK(aw == bw && ah == bh);
        }
    }
}

TEST(hex_pick_returns_the_cell_for_its_middle_and_for_points_around_it) {
  for (int flat = 0; flat < 2; ++flat)
    for (int tile = 16; tile <= 24; tile += 8) {
      HexGeo g = geo(flat, tile, 7, 6);
      Grid grid(GridType::Hex, 7, 6, flat ? HexLayout::Flat : HexLayout::Pointy);
      for (Cell c = 0; c < grid.cellCount(); ++c) {
        int mx, my;
        g.middle(grid.x(c), grid.y(c), mx, my);
        CHECK_EQ(g.pick((float)mx, (float)my), c);
        // Points a third of the way towards each neighbour still pick the cell, two thirds pick the neighbour.
        for (int d = 0; d < 6; ++d) {
          Cell nb = grid.neighbor(c, d);
          if (nb == kNoCell) continue;
          int nx, ny;
          g.middle(grid.x(nb), grid.y(nb), nx, ny);
          CHECK_EQ(g.pick(mx + (nx - mx) * 0.33f, my + (ny - my) * 0.33f), c);
          CHECK_EQ(g.pick(mx + (nx - mx) * 0.67f, my + (ny - my) * 0.67f), nb);
        }
      }
    }
}

TEST(hex_pick_is_nothing_outside_the_map) {
  for (int flat = 0; flat < 2; ++flat) {
    HexGeo g = geo(flat, 24, 5, 4);
    CHECK_EQ(g.pick(-30, 10), kNoCell);
    CHECK_EQ(g.pick(10, -30), kNoCell);
    CHECK_EQ(g.pick((float)g.mapW() + 30, 10), kNoCell);
    CHECK_EQ(g.pick(10, (float)g.mapH() + 30), kNoCell);
    CHECK_EQ(g.pick(-1000, -1000), kNoCell);
  }
  // The notch the half-hexagon shift leaves: pointy, an even row ends half a hexagon before an odd one.
  HexGeo p = geo(0, 24, 5, 4);
  CHECK_EQ(p.pick((float)(5 * 24 + 6), 14.0f), kNoCell);  // row 0 spans 0..120; x = 126 is beyond its last hexagon
  CHECK(p.pick((float)(5 * 24 + 6), 35.0f) != kNoCell);   // row 1 spans 12..132
  // nearestCell never gives up: a point outside gets the nearest hexagon of the map.
  CHECK_EQ(p.nearestCell(-50, -50), 0);
  CHECK_EQ(p.nearestCell(1000, 35), 9);  // row 1's last hexagon (12 + 4 * 24 + 12, 35) is the nearest middle
}

TEST(hex_drawn_hexagons_tile_exactly_and_agree_with_picking) {
  for (int flat = 0; flat < 2; ++flat)
    for (int tile = 16; tile <= 32; tile += 8) {
      const int cols = 6, rows = 6;
      HexGeo g = geo(flat, tile, cols, rows);
      const int W = g.mapW(), H = g.mapH();
      std::vector<int> owner((size_t)(W * H), -1);
      int overlaps = 0;
      for (int y = 0; y < rows; ++y)
        for (int x = 0; x < cols; ++x) {
          int bx, by;
          g.box(x, y, bx, by);
          for (int row = 0; row < g.boxH(); ++row) {
            int l, r;
            g.rowSpan(row, l, r);
            // touches the box on the left, right at some row; spans are inside the box
            CHECK(l >= 0 && r <= g.boxW() && l < r);
            for (int px = bx + l; px < bx + r; ++px) {
              int& o = owner[(size_t)((by + row) * W + px)];
              if (o >= 0) ++overlaps;
              o = y * cols + x;
            }
          }
        }
      CHECK_EQ(overlaps, 0);
      // Inside the map's rectangle, away from the notches at the edges, every pixel belongs to a hexagon and the
      // hexagon is the one picking gives (the sums of the spans' rounding is the only place they could differ).
      int gaps = 0, differ = 0;
      for (int y = g.L; y < H - g.L; ++y)
        for (int x = g.L; x < W - g.L; ++x) {
          int o = owner[(size_t)(y * W + x)];
          if (o < 0) {
            ++gaps;
            continue;
          }
          if (g.pick(x + 0.5f, y + 0.5f) != o) ++differ;
        }
      CHECK_EQ(gaps, 0);
      // The edge of a drawn hexagon is on whole pixels; the nearest-middle rule cuts at the exact line, so a
      // pixel on the line may go either way (under 1% of the pixels in practice). Allow a thin seam, never more.
      CHECK(differ * 50 < (W - 2 * g.L) * (H - 2 * g.L));
      // The hexagon touches its box on all four sides.
      int l0, r0, l1, r1;
      g.rowSpan(0, l0, r0);
      g.rowSpan(g.boxH() - 1, l1, r1);
      CHECK(r0 > l0 && r1 > l1);
      int minL = g.boxW(), maxR = 0;
      for (int row = 0; row < g.boxH(); ++row) {
        int l, r;
        g.rowSpan(row, l, r);
        minL = std::min(minL, l);
        maxR = std::max(maxR, r);
      }
      CHECK(minL == 0 && maxR == g.boxW());
    }
}

TEST(hex_cell_helpers_keep_squares_as_they_were_and_centre_units_on_hexagons) {
  Grid sq(GridType::Square, 5, 4);
  float fx, fy;
  cellCentre(sq, 24, sq.at(2, 3), fx, fy);
  CHECK(fx == 2.5f * 24 && fy == 3.5f * 24);
  cellTopLeft(sq, 24, sq.at(2, 3), fx, fy);
  CHECK(fx == 48.0f && fy == 72.0f);
  CHECK_EQ(cellNearPos(sq, 24, 50.0f, 70.0f), sq.at(2, 3));
  Grid hx(GridType::Hex, 5, 4, HexLayout::Pointy);
  cellCentre(hx, 24, hx.at(1, 1), fx, fy);  // odd row: box left 24 + 12, middle x 48; top 21, middle y 35
  CHECK(fx == 48.0f && fy == 35.0f);
  cellTopLeft(hx, 24, hx.at(1, 1), fx, fy);  // the unit's 24 x 24 box centred on it
  CHECK(fx == 36.0f && fy == 23.0f);
  CHECK_EQ(cellNearPos(hx, 24, 36.0f, 23.0f), hx.at(1, 1));
}

TEST(hex_map_proj_follows_the_hexagons_on_the_fixture_games) {
  for (int flat = 0; flat < 2; ++flat) {
    savetest::Rig rig;
    CHECK(rig.load(ote_test::sourceDir() + (flat ? "/tests/fixtures/hex-flat" : "/tests/fixtures/hex-pointy"), "skirmish"));
    rig.start();
    MapProj proj;
    proj.init(rig.battle, nullptr, 24);
    CHECK(proj.hex());
    CHECK(!proj.iso() && !proj.view3d());
    // 12 x 9 cells
    CHECK_EQ(proj.worldW(), flat ? 11 * 21 + 28 : 12 * 24 + 12);
    CHECK_EQ(proj.worldH(), flat ? 9 * 24 + 12 : 8 * 21 + 28);
    const Grid& g = rig.battle.grid;
    for (Cell c = 0; c < g.cellCount(); ++c) {
      int bx, by, bw, bh;
      proj.cellBox(g.x(c), g.y(c), bx, by, bw, bh);
      CHECK(bw == (flat ? 28 : 24) && bh == (flat ? 24 : 28));
      CHECK_EQ(proj.pick(bx + bw / 2, by + bh / 2), c);
    }
    CHECK_EQ(proj.pick(-3, 5), kNoCell);
    CHECK_EQ(proj.pick(proj.worldW() + 2, 5), kNoCell);
    // A square game's projection is what it was.
    savetest::Rig sq;
    CHECK(sq.loadPlain("twin_rivers"));
    sq.start();
    MapProj sp;
    sp.init(sq.battle, nullptr, 24);
    CHECK(!sp.hex());
    CHECK_EQ(sp.worldW(), sq.battle.grid.width() * 24);
    CHECK_EQ(sp.pick(25, 49), sq.battle.grid.at(1, 2));
  }
}
