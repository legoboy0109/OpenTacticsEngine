// A read-only isometric picture of the map being painted (the "2D isometric" look of docs/VIEWS.md, docs/HEX.md): every square or
// hexagon raised to its height, side faces where the next square lies lower, the game's own terrain pictures on the top faces,
// units / structures / deployment slots as markers. The projection is the engine's own pure maths (core/view/iso.h for squares,
// core/view/hex_iso.h for hexagons; no SDL, no drawing code of the player), so the picture agrees with what the player shows.
// It redraws whenever the map changes (also while a stroke is under way) and always fits the whole map into the pane.
#pragma once
#include <QWidget>
#include <map>
#include <vector>

#include "map_art.h"
#include "map_markers.h"
#include "map_painter.h"

namespace ote_native {

class IsoPreview : public QWidget {
  Q_OBJECT
 public:
  IsoPreview(MapPainter* painter, MapArt* art, QWidget* parent = nullptr);

  // Where the middle of a cell's top face is in widget pixels (for tests); false when the map is not shown.
  bool cellTop(int x, int y, QPointF* out);
  QSize sizeHint() const override { return QSize(360, 300); }

 protected:
  void paintEvent(QPaintEvent*) override;

 private:
  struct Style { QColor color; QPixmap art; };
  void drawSquare(QPainter& p, const ote_editor::MapGrid& g, const std::map<char, Style>& styles, double scale, const QPointF& origin);
  void drawHex(QPainter& p, const ote_editor::MapGrid& g, const std::map<char, Style>& styles, double scale, const QPointF& origin);
  void drawMarkers(QPainter& p, const std::vector<const MapMarker*>& here, const QPointF& mid, double scale);
  std::map<char, Style> stylesFor(const ote_editor::MapGrid& g);
  void fit(const ote_editor::MapGrid& g, double* scale, QPointF* origin, int* levels);

  MapPainter* painter_;
  MapArt* art_;
  std::vector<MapMarker> markers_;
  bool markersStale_ = true;
};

}  // namespace ote_native
