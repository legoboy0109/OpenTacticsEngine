#include "battle/map_proj.h"

#include <algorithm>
#include <cmath>

namespace ote {

void MapProj::init(const Battle& b, const StyleDef* style, int tile) {
  tile_ = tile;
  gridW_ = b.grid.width();
  gridH_ = b.grid.height();
  iso_ = style && style->view == StyleView::Isometric;
  iso3d_ = style && style->view == StyleView::Isometric3D;
  unitW_ = tile;
  elev_.assign((size_t)(gridW_ * gridH_), 0);
  if (!raised()) return;
  // Heights count from the lowest square; a wild value in a map file must not
  // push the map off the screen, so the range is capped.
  int lo = 0, hi = 0;
  for (size_t i = 0; i < elev_.size(); ++i) {
    int h = b.cells[i].height;
    lo = i ? std::min(lo, h) : h;
    hi = i ? std::max(hi, h) : h;
  }
  const int kMaxLevels = 32;
  for (size_t i = 0; i < elev_.size(); ++i) elev_[i] = std::min(b.cells[i].height - lo, kMaxLevels);
  const IsoConfig& ic = style->iso;
  geo_ = IsoGeometry();
  geo_.tileW = ic.tileWidth > 0 ? ic.tileWidth : tile * 2;
  geo_.tileH = ic.tileHeight > 0 ? ic.tileHeight : geo_.tileW / 2;
  geo_.tileH += geo_.tileH % 2;
  geo_.step = ic.heightStep >= 0 ? ic.heightStep : geo_.tileH / 2;
  geo_.gridW = gridW_;
  geo_.gridH = gridH_;
  geo_.levels = std::min(hi - lo, kMaxLevels);
  unitW_ = ic.unitWidth > 0 ? ic.unitWidth : geo_.tileW / 2;
  geo_.headroom = std::max(geo_.tileH, unitW_ * 2);  // room for a unit or a tall picture on the top corner
  // The 3D isometric view takes the same measures, so a style can switch kind and keep its proportions.
  geo3_ = Iso3DGeometry();
  geo3_.setFromDiamond(geo_.tileW, geo_.tileH, geo_.step);
  geo3_.gridW = gridW_;
  geo3_.gridH = gridH_;
  geo3_.levels = geo_.levels;
  geo3_.headroom = geo_.headroom;
}

float MapProj::elevationAt(float gx, float gy) const {
  if (!raised() || gridW_ == 0) return 0;
  // Blend the four squares around the spot (units slide between square middles).
  float fx = std::max(0.0f, std::min(gx - 0.5f, (float)(gridW_ - 1)));
  float fy = std::max(0.0f, std::min(gy - 0.5f, (float)(gridH_ - 1)));
  int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
  int x1 = std::min(x0 + 1, gridW_ - 1), y1 = std::min(y0 + 1, gridH_ - 1);
  float tx = fx - x0, ty = fy - y0;
  float top = elevation(x0, y0) * (1 - tx) + elevation(x1, y0) * tx;
  float bottom = elevation(x0, y1) * (1 - tx) + elevation(x1, y1) * tx;
  return top * (1 - ty) + bottom * ty;
}

void MapProj::toWorld(float fx, float fy, float& wx, float& wy) const {
  if (!raised()) {
    wx = fx;
    wy = fy;
    return;
  }
  float gx = fx / tile_, gy = fy / tile_;
  if (iso3d_) geo3_.project(gx, gy, elevationAt(gx, gy), wx, wy);
  else geo_.project(gx, gy, elevationAt(gx, gy), wx, wy);
}

Cell MapProj::pick(int wx, int wy) const {
  if (iso3d_) {
    int c = geo3_.pick(wx + 0.5f, wy + 0.5f, elev_);
    return c < 0 ? kNoCell : (Cell)c;
  }
  if (iso_) {
    int c = geo_.pick(wx + 0.5f, wy + 0.5f, elev_);
    return c < 0 ? kNoCell : (Cell)c;
  }
  if (wx < 0 || wy < 0 || wx >= gridW_ * tile_ || wy >= gridH_ * tile_) return kNoCell;
  return (Cell)((wy / tile_) * gridW_ + wx / tile_);
}

void MapProj::cellBox(int x, int y, int& bx, int& by, int& bw, int& bh) const {
  if (iso3d_) {  // the box around the top face's four corners
    float lo[2] = {1e9f, 1e9f}, hi[2] = {-1e9f, -1e9f};
    for (int i = 0; i < 4; ++i) {
      float wx, wy;
      geo3_.project((float)(x + (i & 1)), (float)(y + (i >> 1)), (float)elevation(x, y), wx, wy);
      lo[0] = std::min(lo[0], wx);
      lo[1] = std::min(lo[1], wy);
      hi[0] = std::max(hi[0], wx);
      hi[1] = std::max(hi[1], wy);
    }
    bx = (int)std::lround(lo[0]);
    by = (int)std::lround(lo[1]);
    bw = (int)std::lround(hi[0]) - bx;
    bh = (int)std::lround(hi[1]) - by;
    return;
  }
  if (!iso_) {
    bx = x * tile_;
    by = y * tile_;
    bw = bh = tile_;
    return;
  }
  float nx, ny;
  geo_.project((float)x, (float)y, (float)elevation(x, y), nx, ny);
  bx = (int)std::lround(nx) - geo_.tileW / 2;
  by = (int)std::lround(ny);
  bw = geo_.tileW;
  bh = geo_.tileH;
}

}  // namespace ote
