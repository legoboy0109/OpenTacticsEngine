// MapView::paintEvent: terrain, tile layer, heights, markers and the tool's own feedback (hover, rectangle preview).
#include <QPaintEvent>
#include <QPainter>
#include <algorithm>
#include <cmath>

#include "map_view.h"

namespace ote_native {

namespace {
// Pixel-exact box of a square cell so neighbours never leave a seam or overlap at fractional zoom.
QRect snapped(const QRectF& r) {
  const int l = (int)std::lround(r.left()), t = (int)std::lround(r.top());
  return QRect(QPoint(l, t), QPoint((int)std::lround(r.right()) - 1, (int)std::lround(r.bottom()) - 1));
}
}  // namespace

void MapView::paintMessage(QPainter& p, const QString& text) {
  p.setPen(palette().color(QPalette::WindowText));
  p.drawText(viewport()->rect().adjusted(20, 20, -20, -20), Qt::AlignCenter | Qt::TextWordWrap, text);
}

void MapView::paintEvent(QPaintEvent* ev) {
  QPainter p(viewport());
  p.fillRect(ev->rect(), palette().color(QPalette::Window).darker(118));
  if (!painter_->valid()) {
    paintMessage(p, tr("This map's tile grid cannot be shown here:\n%1\n\nThe Tree page still shows every key.").arg(painter_->error()));
    return;
  }
  const ote_editor::MapGrid& g = painter_->grid();
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, false);  // pixel art stays crisp

  // One style per legend symbol, built once per paint.
  std::map<std::string, TerrainInfo> infos;
  for (const TerrainInfo& t : painter_->terrains()) infos[t.id] = t;
  QHash<char, CellStyle> styles;
  for (const auto& e : g.legend) {
    CellStyle s;
    const auto it = infos.find(e.second);
    s.color = it == infos.end() ? QColor(200, 60, 200) : it->second.color;
    s.art = art_->terrainTile(e.second);
    s.letter = QChar(e.first);
    styles.insert(e.first, s);
  }

  const QRectF vis = QRectF(toScene(ev->rect().topLeft()), toScene(ev->rect().bottomRight())).normalized();
  int x0, y0, x1, y1;
  geom_.nearestCell(vis.topLeft(), &x0, &y0);
  geom_.nearestCell(vis.bottomRight(), &x1, &y1);
  x0 = std::max(0, x0 - 1);
  y0 = std::max(0, y0 - 1);
  x1 = std::min(g.width - 1, x1 + 1);
  y1 = std::min(g.height - 1, y1 + 1);

  const double cellPx = MapGeometry::kCell * zoom_;
  const bool drawLines = cellPx >= 9;
  QFont font = p.font();
  font.setPixelSize(std::max(6, (int)(cellPx * 0.45)));
  QFont small = p.font();
  small.setPixelSize(std::max(6, (int)(cellPx * 0.28)));
  const bool layerOn = showLayer_ && g.hasLayer && !g.hex;

  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x) {
      const QRectF b = geom_.bounds(x, y);
      const QRect box = snapped(QRectF(toViewport(b.topLeft()), b.size() * zoom_));
      QPolygonF poly;
      if (g.hex) {
        for (const QPointF& pt : geom_.outline(x, y)) poly << toViewport(pt);
      }
      const auto it = styles.constFind(g.symbolAt(x, y));
      CellStyle st;
      st.color = QColor(200, 60, 200);
      st.letter = QLatin1Char('?');
      if (it != styles.constEnd()) st = *it;
      QPixmap art = st.art;
      if (layerOn && g.layerAt(x, y) >= 0) {
        const QPixmap tile = art_->layerTile(g.layerTileset, g.layerAt(x, y));
        if (!tile.isNull()) art = tile;
      }
      p.setPen(Qt::NoPen);
      p.setBrush(st.color);
      if (g.hex) p.drawPolygon(poly);
      else p.drawRect(box.adjusted(0, 0, 1, 1));
      if (!art.isNull()) {
        p.drawPixmap(box.adjusted(0, 0, 1, 1), art);
      } else if (cellPx >= 14) {
        p.setFont(font);
        p.setPen(QColor(0, 0, 0, 150));
        p.drawText(box, Qt::AlignCenter, st.letter);
      }
      if (showHeights_ && g.hasHeights && g.heightAt(x, y) > 0) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, std::min(150, 30 + g.heightAt(x, y) * 26)));
        if (g.hex) p.drawPolygon(poly);
        else p.drawRect(box.adjusted(0, 0, 1, 1));
      }
      if (drawLines) {
        p.setPen(QColor(0, 0, 0, 70));
        p.setBrush(Qt::NoBrush);
        if (g.hex) p.drawPolygon(poly);
        else p.drawRect(box);
      }
      if (showHeights_ && g.hasHeights && cellPx >= 20) {
        p.setFont(small);
        p.setPen(g.heightAt(x, y) > 0 ? QColor(20, 20, 20) : QColor(0, 0, 0, 70));
        p.drawText(box.adjusted(2, 1, -3, -1), Qt::AlignRight | Qt::AlignBottom, QString::number(g.heightAt(x, y)));
      }
    }

  // Rectangle tool: tint every cell the drag would paint.
  if (stroking_ && tool_ == MapTool::Rectangle) {
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 0, 80));
    for (int y = std::min(rectY0_, rectY1_); y <= std::max(rectY0_, rectY1_); ++y)
      for (int x = std::min(rectX0_, rectX1_); x <= std::max(rectX0_, rectX1_); ++x) {
        QPolygonF poly;
        for (const QPointF& pt : geom_.outline(x, y)) poly << toViewport(pt);
        p.drawPolygon(poly);
      }
  }

  // Markers (read-only context): structures first so a unit standing on one stays visible.
  refreshMarkers();
  const double u = cellPx;
  auto draw = [&](const MapMarker& m, MapMarker::Kind kind) {
    if (m.kind != kind || !g.inside(m.x, m.y)) return;
    const QPointF c = toViewport(geom_.center(m.x, m.y));
    p.setFont(font);
    p.setPen(QPen(QColor(0, 0, 0, 200), 1.5));
    p.setBrush(m.color);
    if (kind == MapMarker::Kind::Unit) {
      p.drawEllipse(c, u * 0.3, u * 0.3);
      p.setPen(QColor(255, 255, 255));
      p.drawText(QRectF(c.x() - u * 0.3, c.y() - u * 0.3, u * 0.6, u * 0.6), Qt::AlignCenter, m.letter);
    } else if (kind == MapMarker::Kind::Structure) {
      QRectF r(c.x() - u * 0.34, c.y() - u * 0.34, u * 0.68, u * 0.68);
      if (!g.hex && (m.w > 1 || m.h > 1)) {  // covers several squares: outline the whole footprint
        const QRectF a = geom_.bounds(m.x, m.y), z = geom_.bounds(std::min(m.x + m.w - 1, g.width - 1), std::min(m.y + m.h - 1, g.height - 1));
        r = QRectF(toViewport(a.topLeft()), toViewport(z.bottomRight())).adjusted(u * 0.08, u * 0.08, -u * 0.08, -u * 0.08);
      }
      if (g.hex && m.rings > 0) {  // the middle hexagon and its ring
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(m.color, 2, Qt::DashLine));
        p.drawEllipse(c, u * 1.5, u * 1.5);
        p.setBrush(m.color);
        p.setPen(QPen(QColor(0, 0, 0, 200), 1.5));
      }
      p.drawRoundedRect(r, u * 0.1, u * 0.1);
      p.setPen(QColor(255, 255, 255));
      p.drawText(r, Qt::AlignCenter, m.letter);
    } else {  // deployment slot: a small diamond
      QPolygonF d;
      d << QPointF(c.x(), c.y() - u * 0.22) << QPointF(c.x() + u * 0.22, c.y()) << QPointF(c.x(), c.y() + u * 0.22) << QPointF(c.x() - u * 0.22, c.y());
      p.drawPolygon(d);
    }
  };
  for (const MapMarker& m : markers_)
    if (showStructures_) draw(m, MapMarker::Kind::PrepSlot);
  for (const MapMarker& m : markers_)
    if (showStructures_) draw(m, MapMarker::Kind::Structure);
  for (const MapMarker& m : markers_)
    if (showUnits_) draw(m, MapMarker::Kind::Unit);

  if (g.inside(hoverX_, hoverY_)) {  // the cell under the pointer
    QPolygonF poly;
    for (const QPointF& pt : geom_.outline(hoverX_, hoverY_)) poly << toViewport(pt);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 230, 0), 2));
    p.drawPolygon(poly);
  }
}

}  // namespace ote_native
