// Grid::line, the straight line of cells that line of sight past a closed door follows (docs/SRPG.md section 10).
#include <cstdlib>

#include "core/grid/grid.h"
#include "test.h"

using namespace ote;

namespace {

// Both ends are in it and each step goes to a neighbour (hexagons) or to one of the eight surrounding squares (squares).
bool wellFormed(const Grid& g, Cell a, Cell b, const std::vector<Cell>& line) {
  if (line.empty() || line.front() != a || line.back() != b) return false;
  for (size_t i = 1; i < line.size(); ++i) {
    if (line[i] == kNoCell || line[i - 1] == kNoCell) return false;
    if (g.isHex()) {
      if (g.distance(line[i - 1], line[i]) != 1) return false;
    } else if (std::abs(g.x(line[i]) - g.x(line[i - 1])) > 1 || std::abs(g.y(line[i]) - g.y(line[i - 1])) > 1) {
      return false;
    }
  }
  return true;
}

}  // namespace

TEST(grid_line_squares_hexagons_pointy_and_flat) {
  const Grid grids[] = {Grid(GridType::Square, 12, 9), Grid(GridType::Hex, 12, 9, HexLayout::Pointy), Grid(GridType::Hex, 12, 9, HexLayout::Flat)};
  for (const Grid& g : grids) {
    for (Cell a = 0; a < g.cellCount(); a += 7)
      for (Cell b = 0; b < g.cellCount(); b += 5) {
        std::vector<Cell> line;
        g.line(a, b, line);
        CHECK(wellFormed(g, a, b, line));
        if (g.isHex()) CHECK_EQ((int)line.size(), g.distance(a, b) + 1);
        std::vector<Cell> back;
        g.line(b, a, back);  // the same cells walked the other way (a closed door blocks in both directions)
        if (!g.isHex()) CHECK_EQ(back.size(), line.size());
      }
  }
  // A row and a column are straight.
  const Grid sq(GridType::Square, 10, 6);
  std::vector<Cell> row;
  sq.line(sq.at(1, 2), sq.at(6, 2), row);
  CHECK_EQ(row.size(), 6u);
  for (size_t i = 0; i < row.size(); ++i) CHECK(sq.y(row[i]) == 2 && sq.x(row[i]) == 1 + (int)i);
  std::vector<Cell> one;
  sq.line(sq.at(3, 3), sq.at(3, 3), one);
  CHECK(one.size() == 1 && one[0] == sq.at(3, 3));
  sq.line(sq.at(3, 3), kNoCell, one);
  CHECK(one.empty());
}
