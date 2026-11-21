#include "map_geometry.h"

#include <algorithm>
#include <cmath>

namespace ote_native {

namespace {
const double kSqrt3 = std::sqrt(3.0);

// Cube rounding of fractional axial coordinates (the standard way to find the hexagon a point lies in).
void roundAxial(double q, double r, int* oq, int* orr) {
  const double s = -q - r;
  double rq = std::round(q), rr = std::round(r), rs = std::round(s);
  const double dq = std::fabs(rq - q), dr = std::fabs(rr - r), ds = std::fabs(rs - s);
  if (dq > dr && dq > ds) rq = -rr - rs;
  else if (dr > ds) rr = -rq - rs;
  *oq = (int)rq;
  *orr = (int)rr;
}
}  // namespace

// A pointy hexagon is kCell wide and kCell * 2 / sqrt(3) high; a flat one is the other way round.
double MapGeometry::hexW() const { return flat_ ? kCell * 2.0 / kSqrt3 : kCell; }
double MapGeometry::hexH() const { return flat_ ? kCell : kCell * 2.0 / kSqrt3; }

QSizeF MapGeometry::size() const {
  if (!hex_) return QSizeF(w_ * kCell, h_ * kCell);
  if (flat_) return QSizeF(hexW() * (0.75 * (w_ - 1) + 1), hexH() * (h_ + (w_ > 1 ? 0.5 : 0)));
  return QSizeF(hexW() * (w_ + (h_ > 1 ? 0.5 : 0)), hexH() * (0.75 * (h_ - 1) + 1));
}

QPointF MapGeometry::center(int x, int y) const {
  if (!hex_) return QPointF((x + 0.5) * kCell, (y + 0.5) * kCell);
  if (flat_) return QPointF(hexW() * (0.75 * x + 0.5), hexH() * (y + 0.5 + ((x & 1) ? 0.5 : 0.0)));
  return QPointF(hexW() * (x + 0.5 + ((y & 1) ? 0.5 : 0.0)), hexH() * (0.75 * y + 0.5));
}

QRectF MapGeometry::bounds(int x, int y) const {
  const QPointF c = center(x, y);
  const double w = hex_ ? hexW() : kCell, h = hex_ ? hexH() : kCell;
  return QRectF(c.x() - w / 2, c.y() - h / 2, w, h);
}

QPolygonF MapGeometry::outline(int x, int y) const {
  QPolygonF p;
  if (!hex_) {
    const QRectF r = bounds(x, y);
    p << r.topLeft() << r.topRight() << r.bottomRight() << r.bottomLeft();
    return p;
  }
  const QPointF c = center(x, y);
  const double R = kCell / kSqrt3;  // circumradius: the same for both layouts
  for (int i = 0; i < 6; ++i) {
    const double a = (flat_ ? 0.0 : 30.0) + 60.0 * i;
    const double rad = a * 3.14159265358979323846 / 180.0;
    p << QPointF(c.x() + R * std::cos(rad), c.y() + R * std::sin(rad));
  }
  return p;
}

void MapGeometry::rawCell(const QPointF& p, int* x, int* y) const {
  if (!hex_) {
    *x = (int)std::floor(p.x() / kCell);
    *y = (int)std::floor(p.y() / kCell);
    return;
  }
  const double R = kCell / kSqrt3;  // circumradius
  const double px = p.x() - hexW() / 2, py = p.y() - hexH() / 2;  // relative to the middle of cell (0, 0)
  int q, r;
  if (flat_) {
    roundAxial(2.0 / 3.0 * px / R, (-1.0 / 3.0 * px + kSqrt3 / 3.0 * py) / R, &q, &r);
    *x = q;                       // odd-q: odd columns are shoved down
    *y = r + (q - (q & 1)) / 2;
  } else {
    roundAxial((kSqrt3 / 3.0 * px - 1.0 / 3.0 * py) / R, 2.0 / 3.0 * py / R, &q, &r);
    *x = q + (r - (r & 1)) / 2;   // odd-r: odd rows are shoved right
    *y = r;
  }
}

bool MapGeometry::cellAt(const QPointF& p, int* x, int* y) const {
  int cx, cy;
  rawCell(p, &cx, &cy);
  if (cx < 0 || cy < 0 || cx >= w_ || cy >= h_) return false;
  *x = cx;
  *y = cy;
  return true;
}

void MapGeometry::nearestCell(const QPointF& p, int* x, int* y) const {
  int cx, cy;
  rawCell(p, &cx, &cy);
  *x = std::max(0, std::min(w_ - 1, cx));
  *y = std::max(0, std::min(h_ - 1, cy));
}

}  // namespace ote_native
