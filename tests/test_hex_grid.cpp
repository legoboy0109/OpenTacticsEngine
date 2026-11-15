// Hexagon grids (docs/HEX.md "Coordinates"): neighbour order, distance, within, rings, directions.
#include <algorithm>
#include <set>
#include <utility>

#include "core/grid/grid.h"
#include "test.h"

using namespace ote;

namespace {

typedef std::pair<int, int> XY;

// The neighbours of (x, y) that the contract lists, in order; off-map entries are kept as (-1, -1).
std::vector<XY> expect(const Grid& g, const std::vector<XY>& list) {
  std::vector<XY> out;
  for (const XY& p : list) out.push_back(g.at(p.first, p.second) == kNoCell ? XY(-1, -1) : p);
  return out;
}

std::vector<XY> actual(const Grid& g, int x, int y) {
  std::vector<XY> out;
  for (int i = 0; i < g.directionCount(); ++i) {
    Cell n = g.neighbor(g.at(x, y), i);
    out.push_back(n == kNoCell ? XY(-1, -1) : XY(g.x(n), g.y(n)));
  }
  return out;
}

bool same(const std::vector<XY>& a, const std::vector<XY>& b) { return a == b; }

// Steps on an empty map, by breadth-first search over neighbors().
std::vector<int> bfs(const Grid& g, Cell from) {
  std::vector<int> d((size_t)g.cellCount(), -1);
  std::vector<Cell> queue(1, from);
  d[(size_t)from] = 0;
  for (size_t i = 0; i < queue.size(); ++i) {
    Cell nb[Grid::kMaxNeighbors];
    int n = g.neighbors(queue[i], nb);
    for (int k = 0; k < n; ++k)
      if (d[(size_t)nb[k]] < 0) {
        d[(size_t)nb[k]] = d[(size_t)queue[i]] + 1;
        queue.push_back(nb[k]);
      }
  }
  return d;
}

}  // namespace

TEST(hex_pointy_neighbors_follow_the_contract_order) {
  Grid g(GridType::Hex, 6, 6, HexLayout::Pointy);
  CHECK(g.isHex());
  CHECK_EQ(g.directionCount(), 6);
  // even row: (x, y-1) (x+1, y) (x, y+1) (x-1, y+1) (x-1, y) (x-1, y-1)
  CHECK(same(actual(g, 2, 2), expect(g, {{2, 1}, {3, 2}, {2, 3}, {1, 3}, {1, 2}, {1, 1}})));
  // odd row: (x+1, y-1) (x+1, y) (x+1, y+1) (x, y+1) (x-1, y) (x, y-1)
  CHECK(same(actual(g, 2, 3), expect(g, {{3, 2}, {3, 3}, {3, 4}, {2, 4}, {1, 3}, {2, 2}})));
  // Map edges: off-map directions are kNoCell and neighbors() skips them but keeps the order.
  CHECK(same(actual(g, 0, 0), expect(g, {{0, -1}, {1, 0}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}})));
  CHECK(same(actual(g, 0, 1), expect(g, {{1, 0}, {1, 1}, {1, 2}, {0, 2}, {-1, 1}, {0, 0}})));
  CHECK(same(actual(g, 5, 5), expect(g, {{6, 4}, {6, 5}, {6, 6}, {5, 6}, {4, 5}, {5, 4}})));
  CHECK(same(actual(g, 5, 4), expect(g, {{5, 3}, {6, 4}, {5, 5}, {4, 5}, {4, 4}, {4, 3}})));
  Cell out[Grid::kMaxNeighbors];
  CHECK_EQ(g.neighbors(g.at(2, 2), out), 6);
  CHECK_EQ(g.neighbors(g.at(0, 0), out), 2);
  CHECK_EQ(out[0], g.at(1, 0));  // (1,0) then (0,1): order kept
  CHECK_EQ(out[1], g.at(0, 1));
  CHECK_EQ(g.neighbors(g.at(0, 1), out), 5);
}

TEST(hex_flat_neighbors_follow_the_contract_order) {
  Grid g(GridType::Hex, 6, 6, HexLayout::Flat);
  // even column: (x, y-1) (x+1, y-1) (x+1, y) (x, y+1) (x-1, y) (x-1, y-1)
  CHECK(same(actual(g, 2, 2), expect(g, {{2, 1}, {3, 1}, {3, 2}, {2, 3}, {1, 2}, {1, 1}})));
  // odd column: (x, y-1) (x+1, y) (x+1, y+1) (x, y+1) (x-1, y+1) (x-1, y)
  CHECK(same(actual(g, 3, 2), expect(g, {{3, 1}, {4, 2}, {4, 3}, {3, 3}, {2, 3}, {2, 2}})));
  CHECK(same(actual(g, 0, 0), expect(g, {{0, -1}, {1, -1}, {1, 0}, {0, 1}, {-1, 0}, {-1, -1}})));
  CHECK(same(actual(g, 1, 0), expect(g, {{1, -1}, {2, 0}, {2, 1}, {1, 1}, {0, 1}, {0, 0}})));
  CHECK(same(actual(g, 5, 5), expect(g, {{5, 4}, {6, 5}, {6, 6}, {5, 6}, {4, 6}, {4, 5}})));
  CHECK(same(actual(g, 4, 5), expect(g, {{4, 4}, {5, 4}, {5, 5}, {4, 6}, {3, 5}, {3, 4}})));
  Cell out[Grid::kMaxNeighbors];
  CHECK_EQ(g.neighbors(g.at(2, 2), out), 6);
  CHECK_EQ(g.neighbors(g.at(0, 0), out), 2);
  CHECK_EQ(g.neighbors(g.at(1, 0), out), 5);
  CHECK_EQ(g.neighbor(kNoCell, 0), kNoCell);
  CHECK_EQ(g.neighbor(g.at(1, 1), 6), kNoCell);
  CHECK_EQ(g.neighbor(g.at(1, 1), -1), kNoCell);
}

TEST(hex_square_grids_keep_their_four_directions) {
  Grid g(GridType::Square, 4, 3);
  CHECK(!g.isHex());
  CHECK_EQ(g.directionCount(), 4);
  CHECK_EQ(g.neighbor(g.at(1, 1), 0), g.at(1, 0));  // N
  CHECK_EQ(g.neighbor(g.at(1, 1), 1), g.at(2, 1));  // E
  CHECK_EQ(g.neighbor(g.at(1, 1), 2), g.at(1, 2));  // S
  CHECK_EQ(g.neighbor(g.at(1, 1), 3), g.at(0, 1));  // W
  CHECK_EQ(g.neighbor(g.at(1, 1), 4), kNoCell);
  CHECK_EQ(g.neighbor(g.at(0, 0), 0), kNoCell);
}

TEST(hex_neighbors_are_symmetric_and_directionTo_inverts_neighbor) {
  for (int flat = 0; flat < 2; ++flat) {
    Grid g(GridType::Hex, 7, 6, flat ? HexLayout::Flat : HexLayout::Pointy);
    for (Cell c = 0; c < g.cellCount(); ++c)
      for (int i = 0; i < 6; ++i) {
        Cell n = g.neighbor(c, i);
        if (n == kNoCell) continue;
        CHECK_EQ(g.directionTo(c, n), i);
        CHECK(g.directionTo(n, c) >= 0);
        CHECK_EQ(g.neighbor(n, g.directionTo(n, c)), c);
        CHECK_EQ(g.distance(c, n), 1);
        // Direction i and i + 3 are opposite on a hexagon.
        CHECK_EQ(g.neighbor(n, (i + 3) % 6), c);
      }
    CHECK_EQ(g.directionTo(g.at(0, 0), g.at(5, 5)), -1);
    CHECK_EQ(g.directionTo(g.at(0, 0), g.at(0, 0)), -1);
  }
}

TEST(hex_distance_is_symmetric_and_equals_steps_on_an_empty_map) {
  for (int flat = 0; flat < 2; ++flat)
    for (int shape = 0; shape < 3; ++shape) {
      int w = shape == 0 ? 7 : shape == 1 ? 6 : 5, h = shape == 0 ? 6 : shape == 1 ? 7 : 9;
      Grid g(GridType::Hex, w, h, flat ? HexLayout::Flat : HexLayout::Pointy);
      int bad = 0;
      for (Cell a = 0; a < g.cellCount(); ++a) {
        std::vector<int> steps = bfs(g, a);
        for (Cell b = 0; b < g.cellCount(); ++b) {
          if (g.distance(a, b) != steps[(size_t)b] || g.distance(a, b) != g.distance(b, a)) ++bad;
        }
        CHECK_EQ(g.distance(a, a), 0);
      }
      CHECK_EQ(bad, 0);
    }
}

TEST(hex_within_equals_the_brute_force_set_in_ascending_order) {
  for (int flat = 0; flat < 2; ++flat) {
    Grid g(GridType::Hex, 8, 7, flat ? HexLayout::Flat : HexLayout::Pointy);
    std::vector<Cell> got;
    int bad = 0;
    for (Cell c = 0; c < g.cellCount(); ++c)
      for (int r = -1; r <= 9; ++r) {
        g.within(c, r, got);
        std::vector<Cell> want;
        for (Cell d = 0; d < g.cellCount(); ++d)
          if (r >= 0 && g.distance(c, d) <= r) want.push_back(d);
        if (got != want) ++bad;
      }
    CHECK_EQ(bad, 0);
    g.within(g.at(3, 3), 1, got);
    CHECK_EQ((int)got.size(), 7);  // the cell and its six neighbours
    g.within(kNoCell, 2, got);
    CHECK(got.empty());
  }
}

TEST(hex_ringCells_is_the_middle_then_the_six_neighbours) {
  Grid g(GridType::Hex, 6, 6, HexLayout::Pointy);
  std::vector<Cell> cells;
  CHECK(g.ringCells(g.at(2, 2), 0, cells));
  CHECK_EQ((int)cells.size(), 1);
  CHECK(g.ringCells(g.at(2, 3), 1, cells));
  CHECK_EQ((int)cells.size(), 7);
  CHECK_EQ(cells[0], g.at(2, 3));
  for (int i = 0; i < 6; ++i) CHECK_EQ(cells[(size_t)i + 1], g.neighbor(g.at(2, 3), i));
  CHECK(!g.ringCells(g.at(0, 0), 1, cells));  // a ring off the map edge
  CHECK(cells.empty());
  CHECK(!g.ringCells(g.at(5, 2), 1, cells));
  CHECK(g.ringCells(g.at(5, 2), 0, cells));
  CHECK(!g.ringCells(g.at(2, 2), 2, cells));
  CHECK(!g.ringCells(kNoCell, 0, cells));
  Grid sq(GridType::Square, 6, 6);
  CHECK(!sq.ringCells(sq.at(2, 2), 1, cells));
}
