// The "isometric-3d" kind of view (docs/VIEWS.md): the same isometric picture
// as iso.h, but drawn from real 3D blocks by a camera without perspective
// that turns about the map. Pure maths, shared by the drawer, pointer picking
// and tests; nothing here needs the 3D drawing layer.
//
// 3D space ("squares"): square (x, y) covers X = x - gridW/2 .. +1 and
// Z = y - gridH/2 .. +1; Y is up, one height level = stepY. The camera looks
// at the map's middle from `yaw` (radians about Y) and `pitch` above the
// ground. With turn 0 the picture is the one of the 2D isometric view: grid x
// grows to the lower right, y to the lower left.
//
// World pixels: a square box of side size() around everything the map can
// show at any turn, origin at its top-left, so the scrolling camera needs no
// new limits while the view turns.
#pragma once
#include <cmath>
#include <vector>

namespace ote {

struct Iso3DGeometry {
  float pixels = 34;     // length of a square's edge in pixels (before foreshortening)
  float sinPitch = 0.5f; // camera height angle; 0.5 gives diamonds twice as wide as high
  float stepY = 0.5f;    // height of one level, in squares
  int gridW = 0, gridH = 0;
  int levels = 0;        // highest elevation on the map (>= 0)
  float yaw = 0.7853982f;  // quarter-turn n: (n + 0.5) * pi/2
  int headroom = 0;      // free pixels kept around the map (tall pictures)

  // From the measures of the 2D isometric view (diamond tileW x tileH, a level
  // raises a square `step` pixels), so both kinds line up at turn 0.
  void setFromDiamond(int tileW, int tileH, int step) {
    pixels = tileW * 0.70710678f;
    sinPitch = tileH > 0 && tileH < tileW ? (float)tileH / (float)tileW : 0.5f;
    stepY = step / (pixels * cosPitch());
  }
  static float yawOfTurn(float quarterTurns) { return (quarterTurns + 0.5f) * 1.5707963f; }
  float cosPitch() const { return std::sqrt(1.0f - sinPitch * sinPitch); }

  // Half the side of the world box, in pixels.
  float radius() const {
    float h = (levels + 1) * stepY, r = std::sqrt(gridW * gridW * 0.25f + gridH * gridH * 0.25f + h * h);
    return std::ceil(r * pixels) + (float)headroom;
  }
  int size() const { return (int)radius() * 2; }

  // A point in 3D space -> world pixels.
  void projectPoint(float X, float Y, float Z, float& wx, float& wy) const {
    float s = std::sin(yaw), c = std::cos(yaw), R = radius();
    wx = (c * X - s * Z) * pixels + R;
    wy = ((s * X + c * Z) * sinPitch - Y * cosPitch()) * pixels + R;
  }
  // A grid spot (in squares, like iso.h) at an elevation (in levels).
  void project(float gx, float gy, float elevation, float& wx, float& wy) const {
    projectPoint(gx - gridW * 0.5f, elevation * stepY, gy - gridH * 0.5f, wx, wy);
  }
  // Grid position of a world pixel, assuming it lies on a surface at height Y (in squares).
  void unprojectAt(float wx, float wy, float Y, float& gx, float& gy) const {
    float s = std::sin(yaw), c = std::cos(yaw), R = radius();
    float a = (wx - R) / pixels, d = ((wy - R) / pixels + Y * cosPitch()) / sinPitch;
    gx = c * a + s * d + gridW * 0.5f;
    gy = -s * a + c * d + gridH * 0.5f;
  }

  // The square under a world pixel, or -1: the block nearest the camera whose
  // top or side faces contain the pixel. `elev`: one elevation per square,
  // row by row (0..levels). Blocks reach one level below the lowest ground.
  int pick(float wx, float wy, const std::vector<int>& elev) const;
  // True if blocks nearer the camera cover the middle of square (x, y)'s top face.
  bool hidden(int x, int y, const std::vector<int>& elev) const;
};

}  // namespace ote
