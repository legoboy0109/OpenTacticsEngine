// docs/SRPG.md "Facing", the grid part (core/grid/facing.h): direction names, which way a square lies from another, and the arc of an attack on
// squares and on both hexagon layouts.
#include "core/grid/facing.h"
#include "test.h"

using namespace ote;

namespace {

Cell cellAt(const Grid& g, int x, int y) { return g.at(x, y); }

}  // namespace

TEST(the_directions_have_the_names_a_map_writes) {
  const Grid sq(GridType::Square, 5, 5);
  const Grid pointy(GridType::Hex, 5, 5, HexLayout::Pointy);
  const Grid flat(GridType::Hex, 5, 5, HexLayout::Flat);
  CHECK_STR(directionName(sq, 0), "north");
  CHECK_STR(directionName(sq, 3), "west");
  CHECK_STR(directionName(sq, 4), "");
  CHECK_STR(directionName(pointy, 0), "northeast");
  CHECK_STR(directionName(pointy, 3), "southwest");
  CHECK_STR(directionName(flat, 0), "north");
  CHECK_STR(directionName(flat, 3), "south");
  for (const Grid* g : {&sq, &pointy, &flat})
    for (int d = 0; d < g->directionCount(); ++d) CHECK_EQ(directionFromName(*g, directionName(*g, d)), d);
  CHECK_EQ(directionFromName(sq, "northeast"), -1);   // a square grid has four
  CHECK_EQ(directionFromName(pointy, "north"), -1);   // and a pointy hexagon has no straight north
  CHECK_EQ(directionFromName(flat, "east"), -1);
}

TEST(a_neighbour_lies_in_exactly_the_direction_it_was_found_by_on_every_grid) {
  // This also proves the hexagon step tables of core/grid/facing.cpp agree with Grid::neighbor (both layouts, both parities).
  const Grid sq(GridType::Square, 7, 7);
  const Grid pointy(GridType::Hex, 8, 8, HexLayout::Pointy);
  const Grid flat(GridType::Hex, 8, 8, HexLayout::Flat);
  for (const Grid* g : {&sq, &pointy, &flat})
    for (int y = 2; y <= 4; ++y)
      for (int x = 2; x <= 4; ++x) {
        const Cell c = cellAt(*g, x, y);
        for (int d = 0; d < g->directionCount(); ++d) {
          int out[2];
          CHECK_EQ(directionsToward(*g, c, g->neighbor(c, d), out), 1);
          CHECK_EQ(out[0], d);
          CHECK_EQ(directionToward(*g, c, g->neighbor(c, d)), d);
        }
      }
}

TEST(on_squares_the_dominant_axis_decides_and_a_diagonal_has_two_directions) {
  const Grid g(GridType::Square, 9, 9);
  const Cell c = cellAt(g, 4, 4);
  int out[2];
  CHECK_EQ(directionsToward(g, c, cellAt(g, 8, 3), out), 1);  // 4 east, 1 north: east
  CHECK_EQ(out[0], 1);
  CHECK_EQ(directionsToward(g, c, cellAt(g, 3, 0), out), 1);  // 1 west, 4 north: north
  CHECK_EQ(out[0], 0);
  CHECK_EQ(directionsToward(g, c, cellAt(g, 6, 6), out), 2);  // a true diagonal: south and east, the lower number first
  CHECK_EQ(out[0], 1);
  CHECK_EQ(out[1], 2);
  CHECK_EQ(directionsToward(g, c, c, out), 0);
  CHECK_EQ(directionToward(g, c, c), -1);
  CHECK_EQ(directionToward(g, c, cellAt(g, 2, 2)), 0);  // north-west diagonal: north (0) before west (3)
}

TEST(a_square_unit_has_a_front_a_back_and_two_sides) {
  const Grid g(GridType::Square, 9, 9);
  const Cell c = cellAt(g, 4, 4);
  // Facing north (0): the attacker on each side.
  CHECK_EQ(arcOf(g, c, 0, cellAt(g, 4, 3)), kArcFront);
  CHECK_EQ(arcOf(g, c, 0, cellAt(g, 4, 6)), kArcBack);
  CHECK_EQ(arcOf(g, c, 0, cellAt(g, 5, 4)), kArcSide);
  CHECK_EQ(arcOf(g, c, 0, cellAt(g, 3, 4)), kArcSide);
  // Far away on the same line, and off the line (the dominant axis).
  CHECK_EQ(arcOf(g, c, 0, cellAt(g, 4, 0)), kArcFront);
  CHECK_EQ(arcOf(g, c, 0, cellAt(g, 5, 8)), kArcBack);   // 1 east, 4 south
  CHECK_EQ(arcOf(g, c, 0, cellAt(g, 8, 5)), kArcSide);   // 4 east, 1 south: east
  // Facing east (1): front is east.
  CHECK_EQ(arcOf(g, c, 1, cellAt(g, 5, 4)), kArcFront);
  CHECK_EQ(arcOf(g, c, 1, cellAt(g, 3, 4)), kArcBack);
  CHECK_EQ(arcOf(g, c, 1, cellAt(g, 4, 3)), kArcSide);
  // The tie rule: a true diagonal counts as the milder of its two arcs, so it is never a back attack here.
  CHECK_EQ(arcOf(g, c, 0, cellAt(g, 5, 3)), kArcFront);  // north-east of a north-facer: front or side: front
  CHECK_EQ(arcOf(g, c, 0, cellAt(g, 5, 5)), kArcSide);   // south-east: side or back: side
  CHECK_EQ(arcOf(g, c, 2, cellAt(g, 3, 3)), kArcSide);   // facing south, attacker north-west: back or side: side
  CHECK_EQ(arcOf(g, c, 0, c), kArcFront);                // the same square
  CHECK_EQ(arcOf(g, c, -1, cellAt(g, 4, 6)), kArcFront); // no facing: front
}

TEST(a_hexagon_unit_has_three_front_directions_one_back_and_two_sides) {
  for (HexLayout layout : {HexLayout::Pointy, HexLayout::Flat}) {
    const Grid g(GridType::Hex, 9, 9, layout);
    const Cell c = cellAt(g, 4, 4);
    for (int facing = 0; facing < 6; ++facing)
      for (int d = 0; d < 6; ++d) {
        const int rel = (d - facing + 6) % 6;
        const int want = (rel == 0 || rel == 1 || rel == 5) ? kArcFront : rel == 3 ? kArcBack : kArcSide;
        CHECK_EQ(arcOf(g, c, facing, g.neighbor(c, d)), want);
        CHECK_EQ(arcOfDirection(g, facing, d), want);
      }
  }
}

TEST(a_hexagon_attacker_far_away_takes_the_nearest_direction_and_a_boundary_the_milder_arc) {
  const Grid g(GridType::Hex, 12, 12, HexLayout::Pointy);
  const Cell c = cellAt(g, 5, 5);
  // Three steps straight along direction 1 (east): still direction 1.
  Cell far = c;
  for (int i = 0; i < 3; ++i) far = g.neighbor(far, 1);
  int out[2];
  CHECK_EQ(directionsToward(g, c, far, out), 1);
  CHECK_EQ(out[0], 1);
  CHECK_EQ(arcOf(g, c, 1, far), kArcFront);
  CHECK_EQ(arcOf(g, c, 4, far), kArcBack);
  // Two steps along direction 1 and two along direction 2: exactly between them (two directions).
  Cell mid = c;
  for (int i = 0; i < 2; ++i) mid = g.neighbor(mid, 1);
  for (int i = 0; i < 2; ++i) mid = g.neighbor(mid, 2);
  CHECK_EQ(directionsToward(g, c, mid, out), 2);
  CHECK_EQ(out[0], 1);
  CHECK_EQ(out[1], 2);
  // Facing the opposite way of direction 1 (4): direction 1 is the back, direction 2 is a side: the milder arc, side, counts.
  CHECK_EQ(arcOf(g, c, 4, mid), kArcSide);
  CHECK_EQ(arcOf(g, c, 1, mid), kArcFront);
}
