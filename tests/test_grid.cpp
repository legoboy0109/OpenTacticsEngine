#include "core/grid/grid.h"
#include "test.h"

using namespace ote;

TEST(grid_square_neighbors_and_distance) {
  Grid g(GridType::Square, 4, 3);
  CHECK_EQ(g.cellCount(), 12);
  CHECK_EQ(g.at(4, 0), kNoCell);
  CHECK_EQ(g.at(-1, 2), kNoCell);
  Cell out[Grid::kMaxNeighbors];
  CHECK_EQ(g.neighbors(g.at(0, 0), out), 2);  // corner
  CHECK_EQ(g.neighbors(g.at(1, 1), out), 4);  // interior
  CHECK_EQ(g.distance(g.at(0, 0), g.at(3, 2)), 5);
  CHECK_EQ(g.x(g.at(3, 2)), 3);
  CHECK_EQ(g.y(g.at(3, 2)), 2);
}
