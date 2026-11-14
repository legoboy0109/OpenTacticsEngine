// The "3d" kind of view (docs/VIEWS.md "Full 3D"): the block map of the 3D
// isometric view under a camera with perspective that the player turns,
// tilts and zooms freely. Pure maths, shared by the drawer, pointer picking
// and tests; nothing here needs the 3D drawing layer.
//
// 3D space is the one of iso3d.h: square (x, y) covers X = x - gridW/2 .. +1
// and Z = y - gridH/2 .. +1; Y is up, one height level = stepY.
//
// The camera circles a focus point on the map: `yaw` (radians about Y; 0 =
// the camera stands south of the focus and looks north, so grid y goes up
// the screen and grid x to the right), `pitch` above the ground, `distance`
// in squares. Pixels are those of the viewport (viewW x viewH), origin at its
// top-left: this view has no separate scrolling, the focus moves instead.
#pragma once
#include <cmath>
#include <vector>

namespace ote {

struct Free3DGeometry {
  int gridW = 0, gridH = 0;
  int levels = 0;         // highest elevation on the map (>= 0)
  float stepY = 0.5f;     // height of one level, in squares
  int viewW = 352, viewH = 259;
  float fovY = 0.6108652f;  // full vertical angle (35 degrees)

  float focusX = 0, focusY = 0, focusZ = 0;
  float yaw = 0, pitch = 0.8726646f, distance = 11.0f;

  // Limits the player's camera is kept within. The lowest pitch stays above
  // half the field of view, so every sight line through the screen goes down
  // and meets the ground somewhere.
  static constexpr float kMinPitch = 0.4363323f, kMaxPitch = 1.3089969f;  // 25 .. 75 degrees
  static constexpr float kMinDistance = 4.0f, kMaxDistance = 30.0f;
  static constexpr float kNear = 0.3f;

  void eye(float& X, float& Y, float& Z) const {
    float cp = std::cos(pitch);
    X = focusX + std::sin(yaw) * cp * distance;
    Y = focusY + std::sin(pitch) * distance;
    Z = focusZ + std::cos(yaw) * cp * distance;
  }
  // Pixels a length of one square covers at one square's distance.
  float focal() const { return viewH * 0.5f / std::tan(fovY * 0.5f); }

  // A point in 3D space -> viewport pixels. False (and a spot far off screen)
  // if it lies behind the camera. `depth`: distance along the line of sight.
  bool projectPoint(float X, float Y, float Z, float& sx, float& sy, float* depth = nullptr) const;
  // A grid spot (in squares) at an elevation (in levels).
  bool project(float gx, float gy, float elevation, float& sx, float& sy) const {
    return projectPoint(gx - gridW * 0.5f, elevation * stepY, gy - gridH * 0.5f, sx, sy);
  }
  // Screen pixels one square's length covers at a point (0 behind the camera).
  float pixelsAt(float X, float Y, float Z) const;
  // The sight line through a pixel: direction (not normalised) from the eye.
  void ray(float sx, float sy, float& dx, float& dy, float& dz) const;
  // Where the sight line through a pixel meets the level plane at height Y.
  bool groundAt(float sx, float sy, float Y, float& X, float& Z) const;

  // The square under a pixel, or -1: the first block the sight line meets
  // (top or side). `elev`: one elevation per square, row by row (0..levels).
  // Blocks reach one level below the lowest ground.
  int pick(float sx, float sy, const std::vector<int>& elev) const;
  // True if other blocks stand between the camera and the middle of square (x, y)'s top face.
  bool hidden(int x, int y, const std::vector<int>& elev) const;

  // How far the focus would have to move over the ground (dX, dZ) for the
  // point to lie inside the viewport, `marginX` / `marginY` pixels from its
  // edges. (0, 0) when it already does.
  void shiftIntoView(float X, float Y, float Z, float marginX, float marginY, float& dX, float& dZ) const;
  // Keeps the camera within its limits and the focus on the map.
  void clamp();
  // Whole quarter turns nearest to the yaw, 0..3 (which way the arrow keys go).
  int quarter() const;
};

}  // namespace ote
