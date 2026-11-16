// Hexagon maps in the two 3D kinds of view (docs/HEX.md "3D isometric and full 3D"). Pure maths, no drawing.
//
// The 3D space of a hexagon map is the top-down picture in squares: one top-down `tileSize` is one unit, so a spot of
// the "flat map pixels" (core/view/hex_geo.h) (fx, fy) is at X = fx / T - W/2, Z = fy / T - H/2 where W x H is the
// size of the whole map picture in those units. A block is a six-sided prism over the hexagon, its top at
// elevation * stepY and reaching one level below the lowest ground, exactly like the square blocks of iso3d.h.
// Which hexagon owns a spot over the ground is "the nearest middle" (picking, hiding), as in the top-down picture.
#pragma once
#include <vector>

#include "core/view/hex_geo.h"

namespace ote {

struct HexSpace {
  bool on = false;  // false: a square map (every other member is unused)
  HexGeo geo;

  static HexSpace make(const HexGeo& g) {
    HexSpace s;
    s.on = true;
    s.geo = g;
    return s;
  }
  float width() const { return (float)geo.mapW() / (float)geo.T; }   // the map picture, in squares
  float height() const { return (float)geo.mapH() / (float)geo.T; }
  // Flat map pixels <-> 3D ground position.
  void toSpace(float fx, float fy, float& X, float& Z) const {
    X = fx / (float)geo.T - width() * 0.5f;
    Z = fy / (float)geo.T - height() * 0.5f;
  }
  // Middle of a hexagon, and its corner i (HexGeo::corners order).
  void middle(int x, int y, float& X, float& Z) const;
  void corner(int x, int y, int i, float& X, float& Z) const;
  // Index (y * cols + x) of the hexagon whose block stands at the ground position, -1 if that is off the map.
  int cellAt(float X, float Z) const;
};

}  // namespace ote
