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
  free3d_ = style && style->view == StyleView::Full3D;
  unitW_ = tile;
  hex_ = b.grid.isHex();
  if (hex_) {
    hexGeo_ = HexGeo::of(b.grid, tile);
    if (iso3d_ || free3d_) hexSpace_ = HexSpace::make(hexGeo_);
    grid_ = b.grid;
  }
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
  if (free3d_) {  // no measures to read: sizes are in squares, the camera decides the pixels
    int vw = free_.viewW, vh = free_.viewH;
    free_ = Free3DGeometry();
    free_.viewW = vw;
    free_.viewH = vh;
    free_.gridW = gridW_;
    free_.gridH = gridH_;
    free_.levels = std::min(hi - lo, kMaxLevels);
    if (hex_) free_.hex = HexSpace::make(hexGeo_);
    return;
  }
  const IsoConfig& ic = style->iso;
  if (hex_ && iso3d_) {
    // 3D isometric on hexagons: the 2D isometric view's measures (the squash is fixed at 2:1, `tileWidth` pixels per
    // top-down tileSize, a level raises `heightStep` pixels), so turn 0 is that picture.
    const int tw = ic.tileWidth > 0 ? ic.tileWidth : tile * 2;
    const int step = ic.heightStep >= 0 ? ic.heightStep : tw / 4;
    unitW_ = ic.unitWidth > 0 ? ic.unitWidth : tw / 2;
    geo3_ = Iso3DGeometry();
    geo3_.pixels = (float)tw;
    geo3_.sinPitch = 0.5f;
    geo3_.stepY = step / (geo3_.pixels * geo3_.cosPitch());
    geo3_.gridW = gridW_;
    geo3_.gridH = gridH_;
    geo3_.levels = std::min(hi - lo, kMaxLevels);
    geo3_.headroom = unitW_ * 2;  // room for a unit or a tall picture on the top row
    geo3_.hex = HexSpace::make(hexGeo_);
    geo3_.yaw = Iso3DGeometry::yawOfHexTurn(0);  // turn 0: the top-down map squashed, rows straight across
    return;
  }
  if (hex_) {  // the squash is fixed on hexagons (tileHeight is refused by the loader)
    int tw = ic.tileWidth > 0 ? ic.tileWidth : tile * 2;
    hexIso_ = HexIsoGeometry::make(b.grid, tile, tw, ic.heightStep >= 0 ? ic.heightStep : tw / 4);
    hexIso_.levels = std::min(hi - lo, kMaxLevels);
    unitW_ = ic.unitWidth > 0 ? ic.unitWidth : tw / 2;
    // A taller unit picture needs more room above the top row.
    hexIso_.headroom = std::max(hexIso_.headroom, unitW_ * 2);
    hexOrder_.resize(elev_.size());
    for (size_t i = 0; i < hexOrder_.size(); ++i) hexOrder_[i] = (Cell)i;
    std::stable_sort(hexOrder_.begin(), hexOrder_.end(), [&](Cell a, Cell b2) {
      int ka = hexIso_.sortKey(grid_.x(a), grid_.y(a)), kb = hexIso_.sortKey(grid_.x(b2), grid_.y(b2));
      return ka != kb ? ka < kb : grid_.x(a) < grid_.x(b2);
    });
    return;
  }
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

float MapProj::hexElevationAt(float fx, float fy) const {
  if (!raised() || !hex_ || elev_.empty()) return 0;
  Cell a = hexGeo_.nearestCell(fx, fy);
  int ax, ay;
  hexGeo_.middle(grid_.x(a), grid_.y(a), ax, ay);
  float da = std::hypot(ax - fx, ay - fy), db = 1e30f;
  Cell nb = kNoCell;
  for (int d = 0; d < grid_.directionCount(); ++d) {
    Cell n = grid_.neighbor(a, d);
    if (n == kNoCell) continue;
    int nx, ny;
    hexGeo_.middle(grid_.x(n), grid_.y(n), nx, ny);
    float dist = std::hypot(nx - fx, ny - fy);
    if (dist < db) { db = dist; nb = n; }
  }
  float ea = (float)elev_[(size_t)a];
  if (nb == kNoCell || da + db <= 0) return ea;
  float t = da / (da + db);  // 0 on the middle of `a`, one half on the line between the two
  return ea * (1 - t) + (float)elev_[(size_t)nb] * t;
}

float MapProj::elevationAt(float gx, float gy) const {
  if (!raised() || gridW_ == 0) return 0;
  if (hex_) return hexElevationAt(gx * tile_, gy * tile_);
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
  if (hex_ && iso_) {  // 2D isometric on hexagons
    hexIso_.project(fx, fy, hexElevationAt(fx, fy), wx, wy);
    return;
  }
  float gx = fx / tile_, gy = fy / tile_;
  if (free3d_) free_.project(gx, gy, elevationAt(gx, gy), wx, wy);
  else if (iso3d_) geo3_.project(gx, gy, elevationAt(gx, gy), wx, wy);
  else geo_.project(gx, gy, elevationAt(gx, gy), wx, wy);
}

Cell MapProj::pick(int wx, int wy) const {
  if (free3d_) {
    int c = free_.pick(wx + 0.5f, wy + 0.5f, elev_);
    return c < 0 ? kNoCell : (Cell)c;
  }
  if (iso3d_) {
    int c = geo3_.pick(wx + 0.5f, wy + 0.5f, elev_);
    return c < 0 ? kNoCell : (Cell)c;
  }
  if (iso_ && hex_) {
    int c = hexIso_.pick(wx + 0.5f, wy + 0.5f, elev_);
    return c < 0 ? kNoCell : (Cell)c;
  }
  if (iso_) {
    int c = geo_.pick(wx + 0.5f, wy + 0.5f, elev_);
    return c < 0 ? kNoCell : (Cell)c;
  }
  if (hex_) return hexGeo_.pick(wx + 0.5f, wy + 0.5f);  // the nearest hexagon middle to the pixel's middle
  if (wx < 0 || wy < 0 || wx >= gridW_ * tile_ || wy >= gridH_ * tile_) return kNoCell;
  return (Cell)((wy / tile_) * gridW_ + wx / tile_);
}

void MapProj::cellMid(int x, int y, float& gx, float& gy) const {
  if (!hex_) {
    gx = x + 0.5f;
    gy = y + 0.5f;
    return;
  }
  int mx, my;
  hexGeo_.middle(x, y, mx, my);
  gx = mx / (float)tile_;
  gy = my / (float)tile_;
}

void MapProj::cellBox(int x, int y, int& bx, int& by, int& bw, int& bh) const {
  if (view3d() && hex_) {  // the box around the top face's six corners
    float c[6][2], lo[2] = {1e9f, 1e9f}, hi[2] = {-1e9f, -1e9f};
    hexGeo_.corners(x, y, c);
    for (int i = 0; i < 6; ++i) {
      float wx, wy;
      project3(c[i][0] / tile_ - spaceW() * 0.5f, elevation(x, y) * stepY(), c[i][1] / tile_ - spaceH() * 0.5f, wx, wy);
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
  if (view3d()) {  // the box around the top face's four corners
    float lo[2] = {1e9f, 1e9f}, hi[2] = {-1e9f, -1e9f};
    for (int i = 0; i < 4; ++i) {
      float wx, wy;
      project3(x + (i & 1) - gridW_ * 0.5f, elevation(x, y) * stepY(), y + (i >> 1) - gridH_ * 0.5f, wx, wy);
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
  if (iso_ && hex_) {  // the top face's box
    hexIso_.topBox(x, y, elevation(x, y), bx, by);
    bw = hexIso_.boxW();
    bh = hexIso_.boxH();
    return;
  }
  if (hex_) {  // the hexagon's box (the hexagon touches it on all four sides)
    hexGeo_.box(x, y, bx, by);
    bw = hexGeo_.boxW();
    bh = hexGeo_.boxH();
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
