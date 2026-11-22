#include "map_view.h"

#include <QHelpEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QToolTip>
#include <algorithm>
#include <cmath>

namespace ote_native {

namespace {
constexpr double kMargin = 12;  // blank border around the map, in pixels
constexpr double kMinZoom = 0.1, kMaxZoom = 8.0;
}  // namespace

MapView::MapView(MapPainter* painter, MapArt* art, QWidget* parent) : QAbstractScrollArea(parent), painter_(painter), art_(art), placement_(painter->project(), painter->doc()) {
  setObjectName("mapView");
  setFocusPolicy(Qt::StrongFocus);
  viewport()->setMouseTracking(true);
  viewport()->setCursor(Qt::CrossCursor);
  auto changed = [this] {
    markersStale_ = true;
    syncGeometry();
    viewport()->update();
  };
  connect(painter_, &MapPainter::gridChanged, this, changed);
  connect(art_, &MapArt::artChanged, this, [this] { viewport()->update(); });
  syncGeometry();
}

void MapView::setTool(MapTool t) {
  tool_ = t;
  dragging_ = false;
  viewport()->setCursor(t == MapTool::Inspect ? Qt::PointingHandCursor : Qt::CrossCursor);
}

void MapView::syncGeometry() {
  if (painter_->valid()) {
    const ote_editor::MapGrid& g = painter_->grid();
    geom_ = MapGeometry(g.width, g.height, g.hex, g.flatTop);
  } else {
    geom_ = MapGeometry();
  }
  updateScrollBars();
}

void MapView::refreshMarkers() {
  if (!markersStale_) return;
  markers_ = readMapMarkers(painter_->doc()->root(), painter_->footprints());
  markersStale_ = false;
}

void MapView::updateScrollBars() {
  const QSizeF c = geom_.size() * zoom_;
  const QSize vp = viewport()->size();
  horizontalScrollBar()->setRange(0, std::max(0, (int)std::ceil(c.width() + 2 * kMargin) - vp.width()));
  verticalScrollBar()->setRange(0, std::max(0, (int)std::ceil(c.height() + 2 * kMargin) - vp.height()));
  horizontalScrollBar()->setPageStep(vp.width());
  verticalScrollBar()->setPageStep(vp.height());
  horizontalScrollBar()->setSingleStep(32);
  verticalScrollBar()->setSingleStep(32);
}

QPointF MapView::origin() const {
  const QSizeF c = geom_.size() * zoom_;
  const QSize vp = viewport()->size();
  const double ox = c.width() + 2 * kMargin <= vp.width() ? (vp.width() - c.width()) / 2 : kMargin - horizontalScrollBar()->value();
  const double oy = c.height() + 2 * kMargin <= vp.height() ? (vp.height() - c.height()) / 2 : kMargin - verticalScrollBar()->value();
  return QPointF(ox, oy);
}

void MapView::resizeEvent(QResizeEvent* ev) {
  QAbstractScrollArea::resizeEvent(ev);
  updateScrollBars();
}

void MapView::setZoom(double z, const QPoint& anchor) {
  z = std::max(kMinZoom, std::min(kMaxZoom, z));
  if (z == zoom_) return;
  const QPointF a = anchor.x() < 0 ? QPointF(viewport()->width() / 2.0, viewport()->height() / 2.0) : QPointF(anchor);
  const QPointF s = toScene(a);  // the map point that must stay under the anchor
  zoom_ = z;
  updateScrollBars();
  horizontalScrollBar()->setValue((int)std::lround(kMargin + s.x() * z - a.x()));
  verticalScrollBar()->setValue((int)std::lround(kMargin + s.y() * z - a.y()));
  viewport()->update();
  emit zoomChanged(zoom_);
}

void MapView::fitToWindow() {
  const QSizeF c = geom_.size();
  if (c.isEmpty()) return;
  const QSize vp = viewport()->size();
  setZoom(std::min((vp.width() - 2 * kMargin) / c.width(), (vp.height() - 2 * kMargin) / c.height()));
}

void MapView::wheelEvent(QWheelEvent* ev) {
  const int dy = ev->angleDelta().y();
  if (dy == 0) return;
  zoomBy(std::pow(1.15, dy / 120.0), ev->position().toPoint());
  ev->accept();
}

bool MapView::cellAtViewport(const QPoint& p, int* x, int* y) const { return geom_.cellAt(toScene(p), x, y); }

QPoint MapView::viewportPointOfCell(int x, int y) const { return toViewport(geom_.center(x, y)).toPoint(); }

const MapMarker* MapView::markerAt(int x, int y) {
  refreshMarkers();
  const MapMarker* best = nullptr;
  auto rank = [](const MapMarker& m) { return m.kind == MapMarker::Kind::Unit ? 3 : (m.kind == MapMarker::Kind::Structure ? 2 : 1); };
  for (const MapMarker& m : markers_) {
    const bool wide = m.kind == MapMarker::Kind::Structure && !geom_.hex();
    const bool hit = wide ? (x >= m.x && x < m.x + m.w && y >= m.y && y < m.y + m.h) : (x == m.x && y == m.y);
    if (hit && (!best || rank(m) > rank(*best))) best = &m;
  }
  return best;
}

bool MapView::viewportEvent(QEvent* ev) {
  if (ev->type() == QEvent::ToolTip) {
    auto* he = static_cast<QHelpEvent*>(ev);
    int x, y;
    if (painter_->valid() && cellAtViewport(he->pos(), &x, &y)) {
      if (const MapMarker* m = markerAt(x, y)) {
        QToolTip::showText(he->globalPos(), m->tip, viewport());
        return true;
      }
      const ote_editor::MapGrid& g = painter_->grid();
      const std::string t = g.terrainAt(x, y);
      QToolTip::showText(he->globalPos(), tr("Square %1, %2: %3%4").arg(x).arg(y).arg(t.empty() ? tr("(not in the legend)") : QString::fromStdString(t),
                                                                          g.hasHeights ? tr(", height %1").arg(g.heightAt(x, y)) : QString()), viewport());
      return true;
    }
    QToolTip::hideText();
    return true;
  }
  return QAbstractScrollArea::viewportEvent(ev);
}

// ---------------------------------------------------------------- mouse

void MapView::applyAt(int x, int y) {
  switch (tool_) {
    case MapTool::Paint: painter_->paintCell(x, y, terrain_); break;
    case MapTool::Raise: painter_->bumpHeight(x, y, +1); break;
    case MapTool::Lower: painter_->bumpHeight(x, y, -1); break;
    case MapTool::SetHeight: painter_->setHeight(x, y, heightValue_); break;
    default: break;
  }
}

bool MapView::beginPress(const QPoint& pos) {
  if (!painter_->valid() || painter_->inStroke()) return false;
  int x, y;
  if (!cellAtViewport(pos, &x, &y)) return false;
  const ote_editor::MapGrid& g = painter_->grid();
  switch (tool_) {
    case MapTool::Pick: {
      const std::string t = g.terrainAt(x, y);
      if (!t.empty()) emit terrainPicked(QString::fromStdString(t), g.heightAt(x, y));
      return true;
    }
    case MapTool::Inspect:
      if (const MapMarker* m = markerAt(x, y)) emit markerActivated((int)m->kind, (int)m->index);
      return true;
    case MapTool::Fill:
      if (terrain_.empty()) return true;
      if (painter_->beginStroke(tr("Fill area"))) {
        painter_->floodFill(x, y, terrain_);
        painter_->endStroke();
        emit strokeFinished();
      }
      return true;
    case MapTool::Rectangle:
      if (terrain_.empty()) return true;
      rectX0_ = rectX1_ = x;
      rectY0_ = rectY1_ = y;
      stroking_ = true;
      return true;
    case MapTool::Paint:
    case MapTool::Raise:
    case MapTool::Lower:
    case MapTool::SetHeight: {
      if (tool_ == MapTool::Paint && terrain_.empty()) return true;
      const QString text = tool_ == MapTool::Paint ? tr("Paint terrain") : tool_ == MapTool::Raise ? tr("Raise height") : tool_ == MapTool::Lower ? tr("Lower height") : tr("Set height");
      if (!painter_->beginStroke(text)) return true;
      stroking_ = true;
      lastScene_ = toScene(pos);
      applyAt(x, y);
      return true;
    }
  }
  return false;
}

void MapView::continueStroke(const QPoint& pos) {
  if (tool_ == MapTool::Rectangle) {
    geom_.nearestCell(toScene(pos), &rectX1_, &rectY1_);
    viewport()->update();
    return;
  }
  // A fast drag skips cells between two mouse events: walk the segment in steps smaller than a cell.
  const QPointF cur = toScene(pos);
  const QPointF d = cur - lastScene_;
  const int steps = std::max(1, (int)std::ceil(std::hypot(d.x(), d.y()) / (MapGeometry::kCell * 0.3)));
  for (int i = 1; i <= steps; ++i) {
    int x, y;
    if (geom_.cellAt(lastScene_ + d * (double(i) / steps), &x, &y)) applyAt(x, y);
  }
  lastScene_ = cur;
}

void MapView::mousePressEvent(QMouseEvent* ev) {
  setFocus();
  if (ev->button() == Qt::MiddleButton || (ev->button() == Qt::LeftButton && spaceDown_)) {
    panning_ = true;
    panLast_ = ev->pos();
    viewport()->setCursor(Qt::ClosedHandCursor);
    return;
  }
  if ((tool_ == MapTool::Slots || tool_ == MapTool::MoveUnit) && (ev->button() == Qt::LeftButton || ev->button() == Qt::RightButton)) {
    pressPlacement(ev->pos(), ev->button());
    return;
  }
  if (ev->button() == Qt::LeftButton) beginPress(ev->pos());
}

// Slots tool: click an empty square = new slot, drag a slot = move it, right-click a slot = remove it. Move unit tool: drag a unit.
// Nothing is written until the button comes up (a drag released outside the map or on the same square changes nothing).
bool MapView::pressPlacement(const QPoint& pos, Qt::MouseButton button) {
  emit placementMessage(QString());
  int x, y;
  if (!painter_->valid() || painter_->inStroke() || !cellAtViewport(pos, &x, &y)) return false;
  const ote_editor::MapGrid& g = painter_->grid();
  QString err;
  if (tool_ == MapTool::MoveUnit) {
    const int u = placement_.unitAt(x, y);
    if (button == Qt::LeftButton && u >= 0) {
      dragging_ = true;
      dragKind_ = MapMarker::Kind::Unit;
      dragIndex_ = (size_t)u;
      viewport()->update();
    }
    return true;
  }
  const int s = placement_.slotAt(x, y);
  if (button == Qt::RightButton) {
    if (s >= 0 && !placement_.removeSlot((size_t)s, &err)) emit placementMessage(err);
  } else if (s >= 0) {
    dragging_ = true;
    dragKind_ = MapMarker::Kind::PrepSlot;
    dragIndex_ = (size_t)s;
    viewport()->update();
  } else if (!placement_.addSlot(x, y, g.width, g.height, &err)) {
    emit placementMessage(err);
  }
  return true;
}

void MapView::releasePlacement(const QPoint& pos) {
  dragging_ = false;
  int x, y;
  if (painter_->valid() && cellAtViewport(pos, &x, &y)) {
    const ote_editor::MapGrid& g = painter_->grid();
    QString err;
    const bool ok = dragKind_ == MapMarker::Kind::Unit ? placement_.moveUnit(dragIndex_, x, y, g.width, g.height, &err)
                                                      : placement_.moveSlot(dragIndex_, x, y, g.width, g.height, &err);
    if (!ok && !err.isEmpty()) emit placementMessage(err);
  }
  viewport()->update();
  emit strokeFinished();
}

void MapView::mouseMoveEvent(QMouseEvent* ev) {
  int x = -1, y = -1;
  if (!cellAtViewport(ev->pos(), &x, &y)) x = y = -1;
  if (x != hoverX_ || y != hoverY_) {
    hoverX_ = x;
    hoverY_ = y;
    emit cellHovered(x, y);
    viewport()->update();
  }
  if (panning_) {
    const QPoint d = ev->pos() - panLast_;
    panLast_ = ev->pos();
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - d.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - d.y());
  } else if (stroking_) {
    continueStroke(ev->pos());
  }
}

void MapView::mouseReleaseEvent(QMouseEvent* ev) {
  if (panning_ && (ev->button() == Qt::MiddleButton || ev->button() == Qt::LeftButton)) {
    panning_ = false;
    viewport()->setCursor(spaceDown_ ? Qt::OpenHandCursor : (tool_ == MapTool::Inspect ? Qt::PointingHandCursor : Qt::CrossCursor));
    return;
  }
  if (dragging_ && ev->button() == Qt::LeftButton) {
    releasePlacement(ev->pos());
    return;
  }
  if (ev->button() != Qt::LeftButton || !stroking_) return;
  stroking_ = false;
  if (tool_ == MapTool::Rectangle) {
    if (painter_->beginStroke(tr("Fill rectangle"))) {
      painter_->paintRect(rectX0_, rectY0_, rectX1_, rectY1_, terrain_);
      painter_->endStroke();
    }
  } else {
    painter_->endStroke();
  }
  viewport()->update();
  emit strokeFinished();
}

void MapView::keyPressEvent(QKeyEvent* ev) {
  if (ev->key() == Qt::Key_Space && !ev->isAutoRepeat()) {
    spaceDown_ = true;
    viewport()->setCursor(Qt::OpenHandCursor);
    return;
  }
  QAbstractScrollArea::keyPressEvent(ev);
}

void MapView::keyReleaseEvent(QKeyEvent* ev) {
  if (ev->key() == Qt::Key_Space && !ev->isAutoRepeat()) {
    spaceDown_ = false;
    viewport()->setCursor(tool_ == MapTool::Inspect ? Qt::PointingHandCursor : Qt::CrossCursor);
    return;
  }
  QAbstractScrollArea::keyReleaseEvent(ev);
}

void MapView::leaveEvent(QEvent* ev) {
  hoverX_ = hoverY_ = -1;
  emit cellHovered(-1, -1);
  viewport()->update();
  QAbstractScrollArea::leaveEvent(ev);
}

}  // namespace ote_native
