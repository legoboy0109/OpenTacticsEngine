// Where the squares of the map are on screen, for the kind of view of the
// visual style in use. Units and effects keep living in "flat map pixels"
// (square (x, y) spans x*tile .. (x+1)*tile; that is what BattleView animates);
// this turns those into world pixels (before the camera) and back.
// Top-down: world pixels = flat map pixels. Isometric: core/view/iso.h.
#pragma once
#include <vector>

#include "core/battle/battle.h"
#include "core/view/iso.h"

namespace ote {

class MapProj {
 public:
  void init(const Battle& b, const StyleDef* style, int tile);
  bool iso() const { return iso_; }
  const IsoGeometry& geo() const { return geo_; }
  int tile() const { return tile_; }
  int unitW() const { return unitW_; }
  int worldW() const { return iso_ ? geo_.worldW() : gridW_ * tile_; }
  int worldH() const { return iso_ ? geo_.worldH() : gridH_ * tile_; }

  // Elevation (levels above the map's lowest square) of a square, and of a
  // spot between squares (gx, gy in squares, measured at square middles).
  int elevation(int x, int y) const { return elev_[(size_t)(y * gridW_ + x)]; }
  float elevationAt(float gx, float gy) const;
  const std::vector<int>& elevations() const { return elev_; }

  // A spot in flat map pixels -> world pixels. In the isometric view the spot
  // is lifted to the ground's elevation there.
  void toWorld(float fx, float fy, float& wx, float& wy) const;
  // The square under a world pixel, or kNoCell.
  Cell pick(int wx, int wy) const;
  // Box around a square's top face in world pixels.
  void cellBox(int x, int y, int& bx, int& by, int& bw, int& bh) const;

 private:
  bool iso_ = false;
  int tile_ = 24, unitW_ = 24, gridW_ = 0, gridH_ = 0;
  IsoGeometry geo_;
  std::vector<int> elev_;
};

}  // namespace ote
