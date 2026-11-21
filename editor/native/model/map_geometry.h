// Where a map cell sits on screen, and which cell a point belongs to, for square and hexagon grids. Pure geometry in "map
// units" (a square is kCell units wide); the view multiplies by its zoom. Hexagons follow docs/HEX.md: pointy-top maps draw
// odd rows half a hexagon to the right, flat-top maps draw odd columns half a hexagon down.
#pragma once
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QSizeF>

namespace ote_native {

class MapGeometry {
 public:
  static constexpr double kCell = 32.0;  // a square's side; a hexagon's flat-to-flat width

  MapGeometry() = default;
  MapGeometry(int width, int height, bool hex, bool flatTop) : w_(width), h_(height), hex_(hex), flat_(flatTop) {}

  int width() const { return w_; }
  int height() const { return h_; }
  bool hex() const { return hex_; }

  QSizeF size() const;                    // the whole map
  QPointF center(int x, int y) const;     // middle of a cell
  QRectF bounds(int x, int y) const;      // the box that holds the cell (a square, or a hexagon's bounding box)
  QPolygonF outline(int x, int y) const;  // the cell's corners (4 or 6)
  // The cell under `p`, or false when the point is outside the map (or in the gap beside the edge hexagons).
  bool cellAt(const QPointF& p, int* x, int* y) const;
  // The cell nearest to `p` even outside the map, for clamping a drag; the result is clamped into the map.
  void nearestCell(const QPointF& p, int* x, int* y) const;

 private:
  void rawCell(const QPointF& p, int* x, int* y) const;  // unclamped (may be outside the map)
  double hexW() const;  // bounding box of one hexagon
  double hexH() const;

  int w_ = 0, h_ = 0;
  bool hex_ = false, flat_ = false;
};

}  // namespace ote_native
