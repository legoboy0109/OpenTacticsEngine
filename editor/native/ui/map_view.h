// The map canvas: a custom-painted scroll area (only the cells in view are drawn, so a 100 x 100 map stays fast) that draws the
// grid with the game's tile art or a colour per terrain, shows overlays, and turns mouse strokes into MapPainter strokes.
// Zoom with the wheel (around the pointer), pan with the middle button (or Space + left button) and the scroll bars.
#pragma once
#include <QAbstractScrollArea>
#include <QHash>
#include <QPixmap>
#include <set>

#include "map_art.h"
#include "map_geometry.h"
#include "map_markers.h"
#include "map_painter.h"
#include "map_placement.h"
#include "map_tool.h"

namespace ote_native {

class MapView : public QAbstractScrollArea {
  Q_OBJECT
 public:
  MapView(MapPainter* painter, MapArt* art, QWidget* parent = nullptr);

  MapTool tool() const { return tool_; }
  void setTool(MapTool t);
  const std::string& terrain() const { return terrain_; }
  void setTerrain(const std::string& id) { terrain_ = id; }
  int heightValue() const { return heightValue_; }
  void setHeightValue(int v) { heightValue_ = v; }

  double zoom() const { return zoom_; }
  void setZoom(double z, const QPoint& anchor = QPoint(-1, -1));  // anchor: a viewport point that stays put (default: the middle)
  void zoomBy(double factor, const QPoint& anchor = QPoint(-1, -1)) { setZoom(zoom_ * factor, anchor); }
  void fitToWindow();

  // Overlays
  void setShowUnits(bool on) { showUnits_ = on; viewport()->update(); }
  void setShowStructures(bool on) { showStructures_ = on; viewport()->update(); }
  void setShowHeights(bool on) { showHeights_ = on; viewport()->update(); }
  void setShowLayer(bool on) { showLayer_ = on; viewport()->update(); }
  bool showUnits() const { return showUnits_; }
  bool showStructures() const { return showStructures_; }
  bool showHeights() const { return showHeights_; }
  bool showLayer() const { return showLayer_; }

  // For tests and the status line: the geometry, and the cell under a viewport point (false outside the map).
  const MapGeometry& geometry() const { return geom_; }
  bool cellAtViewport(const QPoint& p, int* x, int* y) const;
  QPoint viewportPointOfCell(int x, int y) const;  // the middle of a cell, in viewport coordinates

 signals:
  void zoomChanged(double zoom);
  void terrainPicked(const QString& terrainId, int height);
  void cellHovered(int x, int y);  // -1, -1 outside the map
  void markerActivated(int kind, int index);  // MapMarker::Kind, index into the map's units / structures / prep.slots
  void strokeFinished();
  void placementMessage(const QString& text);  // why a slot / unit edit was refused (empty: clear)

 protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;
  void wheelEvent(QWheelEvent*) override;
  void keyPressEvent(QKeyEvent*) override;
  void keyReleaseEvent(QKeyEvent*) override;
  void leaveEvent(QEvent*) override;
  bool viewportEvent(QEvent*) override;

 private:
  struct CellStyle { QColor color; QPixmap art; QChar letter; };
  QPointF origin() const;  // where map point (0, 0) is in the viewport
  QPointF toScene(const QPointF& vp) const { return (vp - origin()) / zoom_; }
  QPointF toViewport(const QPointF& scene) const { return origin() + scene * zoom_; }
  void syncGeometry();
  void updateScrollBars();
  bool beginPress(const QPoint& pos);
  void continueStroke(const QPoint& pos);
  void applyAt(int x, int y);
  const MapMarker* markerAt(int x, int y);
  void refreshMarkers();
  void paintMessage(QPainter& p, const QString& text);

  MapPainter* painter_;
  MapArt* art_;
  MapGeometry geom_;
  MapTool tool_ = MapTool::Paint;
  std::string terrain_;
  int heightValue_ = 1;
  double zoom_ = 1.0;
  bool showUnits_ = true, showStructures_ = true, showHeights_ = true, showLayer_ = false;
  std::vector<MapMarker> markers_;  // refreshed after every change of the file
  bool markersStale_ = true;

  // Slots and Move unit tools: a marker being dragged to another square (drawn at the pointer; written on release).
  bool pressPlacement(const QPoint& pos, Qt::MouseButton button);
  void releasePlacement(const QPoint& pos);
  MapPlacement placement_;
  bool dragging_ = false;
  MapMarker::Kind dragKind_ = MapMarker::Kind::Unit;
  size_t dragIndex_ = 0;

  bool stroking_ = false, panning_ = false, spaceDown_ = false;
  QPointF lastScene_;
  QPoint panLast_;
  int rectX0_ = 0, rectY0_ = 0, rectX1_ = 0, rectY1_ = 0;
  int hoverX_ = -1, hoverY_ = -1;
};

}  // namespace ote_native
