// Hexagon maps in the 2D isometric view (docs/HEX.md "2D isometric"). Pure maths, no drawing: the player's
// MapProj, the drawer, picking and the tests share it.
//
// The top-down picture of a hexagon map (core/view/hex_geo.h, "flat map pixels") is seen from one fixed angle: a
// position goes to (x * sx, y * sy) with sy = sx / 2 and sx = tileWidth / tileSize. So the picture is made of whole
// pixels, the squashed hexagons are laid out as a hexagon lattice of their own (`sq`): the box of a squashed hexagon is
//   pointy: tileWidth wide, the multiple of 4 nearest to L * sy tall;
//   flat:   the multiple of 4 nearest to L * sx wide, tileWidth / 2 (rounded up to even) tall,
// (a long side that would leave a one-pixel seam between hexagons moves to the nearer multiple of 4 that does not),
// and rows (pointy) or columns (flat) are 3/4 of the long side apart, exactly as in the top-down picture. Neighbouring
// hexagons therefore share their edges exactly (HexGeo::rowSpan), and a cell's middle in this lattice is exactly its
// top-down middle scaled by (boxW / topdown boxW, boxH / topdown boxH). That is the whole rounding rule.
//
// World pixels: x as in the lattice; y = lattice y - elevation * step + levels * step + headroom (the box of the whole
// map, with room above for the highest block and for tall pictures).
#pragma once
#include <vector>

#include "core/view/hex_geo.h"

namespace ote {

struct HexIsoGeometry {
  HexGeo td;       // the top-down lattice (view.tileSize)
  HexGeo sq;       // the squashed lattice: boxes of the top faces
  int tileW = 48;  // isometric.tileWidth: the drawn width of one top-down tileSize
  int step = 12;   // pixels one height level raises a hexagon
  int levels = 0;  // highest elevation on the map (>= 0)
  int headroom = 0;

  // `tileWidth` even; `step` >= 0.
  static HexIsoGeometry make(const Grid& g, int tile, int tileWidth, int step);

  int boxW() const { return sq.boxW(); }  // the top face's box (the art cell's width)
  int boxH() const { return sq.boxH(); }
  int worldW() const { return sq.mapW(); }
  // Down to the bottom of the lowest hexagons' side faces.
  int worldH() const { return sq.mapH() + levels * step + step + headroom; }

  // Top-left of a hexagon's top-face box in world pixels at an elevation.
  void topBox(int x, int y, int elevation, int& wx, int& wy) const;
  // A spot in flat map pixels (top-down) -> world pixels, lifted to `elevation` (may be fractional).
  void project(float fx, float fy, float elevation, float& wx, float& wy) const;
  // World pixel -> flat map pixels, assuming it lies on a surface at `elevation`.
  void unproject(float wx, float wy, float elevation, float& fx, float& fy) const;

  // Which of a hexagon's six edges show a side face (direction numbers of the contract's order): pointy the two
  // lower slanted ones, flat the lower three. The vertical edges of a pointy hexagon are edge-on and show nothing.
  unsigned visibleFaces() const { return sq.pointy ? (1u << 2 | 1u << 3) : (1u << 2 | 1u << 3 | 1u << 4); }
  // The columns [l, r) of the box that face `dir` shows below the top face (dir must be a visible face).
  void faceSpan(int dir, int& l, int& r) const;

  // Order of drawing: the pixel row of the hexagon's front point on the ground. Bigger = nearer the viewer; equal
  // rows (hexagons that cannot overlap) go left to right.
  int sortKey(int x, int y) const {
    int bx, by;
    sq.box(x, y, bx, by);
    return by + boxH();
  }

  // The hexagon under a world pixel, or -1: the one nearest the viewer whose top face or side faces contain the
  // pixel. Which hexagon a spot of one level belongs to is "the nearest middle" of the top-down picture.
  // `elev`: one elevation per cell, row by row (cols * rows values, each 0..levels).
  int pick(float wx, float wy, const std::vector<int>& elev) const;
  // True if hexagons nearer the viewer rise high enough to cover the middle of cell (x, y)'s top face.
  bool hidden(int x, int y, const std::vector<int>& elev) const;
};

}  // namespace ote
