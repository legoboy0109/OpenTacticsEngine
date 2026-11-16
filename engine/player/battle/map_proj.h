// Where the squares of the map are on screen, for the kind of view of the
// visual style in use. Units and effects keep living in "flat map pixels"
// (square (x, y) spans x*tile .. (x+1)*tile; that is what BattleView animates);
// this turns those into world pixels (before the camera) and back.
// Top-down: world pixels = flat map pixels. Isometric: core/view/iso.h.
// 3D isometric: core/view/iso3d.h (the view can turn: setTurn).
// Full 3D: core/view/free3d.h (world pixels = viewport pixels; the camera's
// focus moves instead of the 2D scrolling camera: cam3()).
// Hexagon maps (docs/HEX.md) are top-down or 2D isometric. Top-down: world pixels = flat map pixels, where a cell is
// its hexagon's middle (core/view/hex_geo.h); the map size, pick() and cellBox() follow the hexagons. 2D isometric
// (iso() is true, hex() too): core/view/hex_iso.h, hexIso(); the 3D kinds are not drawn for hexagons yet.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

#include "core/battle/battle.h"
#include "core/view/free3d.h"
#include "core/view/iso.h"
#include "core/view/hex_geo.h"
#include "core/view/hex_iso.h"
#include "core/view/iso3d.h"

namespace ote {

class MapProj {
 public:
  void init(const Battle& b, const StyleDef* style, int tile);
  bool iso() const { return iso_; }      // the 2D isometric view
  bool iso3d() const { return iso3d_; }  // the 3D isometric view
  bool free3d() const { return free3d_; }  // the full 3D view
  bool view3d() const { return iso3d_ || free3d_; }  // drawn through the 3D layer
  bool raised() const { return iso_ || view3d(); }  // squares are shown at their heights
  const IsoGeometry& geo() const { return geo_; }
  const HexIsoGeometry& hexIso() const { return hexIso_; }  // the 2D isometric view of a hexagon map
  // Hexagon map, 2D isometric: the cells back to front (the order to draw them in).
  const std::vector<Cell>& hexDrawOrder() const { return hexOrder_; }
  const Iso3DGeometry& geo3() const { return geo3_; }
  // 3D isometric only: where the camera is, in quarter turns (fractions while it turns).
  void setTurn(float quarterTurns) { geo3_.yaw = Iso3DGeometry::yawOfTurn(quarterTurns); }
  // Full 3D only: the camera (the scene moves it), and the viewport it draws into.
  Free3DGeometry& cam3() { return free_; }
  const Free3DGeometry& cam3() const { return free_; }
  void setViewport(int w, int h) { free_.viewW = w; free_.viewH = h; }
  bool hex() const { return hex_; }  // a hexagon map
  const HexGeo& hexGeo() const { return hexGeo_; }
  int tile() const { return tile_; }
  int unitW() const { return unitW_; }
  int worldW() const {
    return free3d_ ? free_.viewW : iso3d_ ? geo3_.size() : iso_ ? (hex_ ? hexIso_.worldW() : geo_.worldW()) : hex_ ? hexGeo_.mapW() : gridW_ * tile_;
  }
  int worldH() const {
    return free3d_ ? free_.viewH : iso3d_ ? geo3_.size() : iso_ ? (hex_ ? hexIso_.worldH() : geo_.worldH()) : hex_ ? hexGeo_.mapH() : gridH_ * tile_;
  }

  // --- The 3D kinds (3D isometric and full 3D) share one 3D space, in squares
  // (core/view/iso3d.h); these hide which camera looks at it.
  int gridW() const { return gridW_; }
  int gridH() const { return gridH_; }
  float stepY() const { return free3d_ ? free_.stepY : geo3_.stepY; }  // height of one level
  float yaw() const { return free3d_ ? free_.yaw : geo3_.yaw; }
  // A point in 3D space -> world pixels; false = behind the camera.
  bool project3(float X, float Y, float Z, float& wx, float& wy) const {
    if (free3d_) return free_.projectPoint(X, Y, Z, wx, wy);
    geo3_.projectPoint(X, Y, Z, wx, wy);
    return true;
  }
  // Screen pixels a square's edge covers at a point (the same everywhere without perspective).
  float pixelsAt(float X, float Y, float Z) const { return free3d_ ? free_.pixelsAt(X, Y, Z) : geo3_.pixels; }
  // Art pixels per square: turns the pixel sizes of pictures into sizes in 3D space.
  float artPixels() const { return free3d_ ? tile_ / 0.75f : geo3_.pixels; }
  // How much shorter an upright thing looks from the camera's height angle, and a flat one lying on the ground.
  float uprightSquash() const { return free3d_ ? std::max(0.5f, std::cos(free_.pitch)) : geo3_.cosPitch(); }
  // Share of an upright picture's art height that shows on screen (pictures are drawn taller to make up for
  // the camera's height angle, fully so without perspective).
  float uprightShown() const { return free3d_ ? std::cos(free_.pitch) / uprightSquash() : 1.0f; }
  float flatSquash() const { return free3d_ ? std::sin(free_.pitch) : geo3_.sinPitch; }
  bool hidden3(int x, int y) const { return free3d_ ? free_.hidden(x, y, elev_) : geo3_.hidden(x, y, elev_); }

  // Elevation (levels above the map's lowest square) of a square, and of a
  // spot between squares (gx, gy in squares, measured at square middles).
  int elevation(int x, int y) const { return elev_[(size_t)(y * gridW_ + x)]; }
  float elevationAt(float gx, float gy) const;
  // The same for a hexagon map, at a spot in flat map pixels: the nearest hexagon's, eased towards the next nearest
  // one (units walk from one hexagon to the next at different heights).
  float hexElevationAt(float fx, float fy) const;
  const std::vector<int>& elevations() const { return elev_; }

  // A spot in flat map pixels -> world pixels. In the isometric view the spot
  // is lifted to the ground's elevation there.
  void toWorld(float fx, float fy, float& wx, float& wy) const;
  // The square under a world pixel, or kNoCell.
  Cell pick(int wx, int wy) const;
  // Box around a square's top face in world pixels.
  void cellBox(int x, int y, int& bx, int& by, int& bw, int& bh) const;

 private:
  bool iso_ = false, iso3d_ = false, free3d_ = false;
  bool hex_ = false;  // a hexagon map (always top-down)
  HexGeo hexGeo_;
  HexIsoGeometry hexIso_;
  std::vector<Cell> hexOrder_;
  Grid grid_;  // a copy, for the neighbours of a hexagon (elevation blending)
  int tile_ = 24, unitW_ = 24, gridW_ = 0, gridH_ = 0;
  IsoGeometry geo_;
  Iso3DGeometry geo3_;
  Free3DGeometry free_;
  std::vector<int> elev_;
};

}  // namespace ote
