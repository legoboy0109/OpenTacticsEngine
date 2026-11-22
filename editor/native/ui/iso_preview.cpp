// The engine's headers come first: one of them has a member called `slots`, which Qt's headers turn into a macro.
#include "core/grid/grid.h"
#include "core/view/hex_iso.h"
#include "core/view/iso.h"
#include "iso_preview.h"

#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

#include "map_geometry.h"

namespace ote_native {

namespace {
constexpr int kTileW = 48, kTileH = 24, kStep = 12;  // the engine's default look (data/graphics.json "isometric" can change it in the player)
constexpr double kMargin = 10;

int maxHeight(const ote_editor::MapGrid& g) {
  int m = 0;
  if (g.hasHeights)
    for (int h : g.heights) m = std::max(m, h);
  return m;
}

ote::IsoGeometry squareGeo(const ote_editor::MapGrid& g) {
  ote::IsoGeometry geo;
  geo.tileW = kTileW;
  geo.tileH = kTileH;
  geo.step = kStep;
  geo.gridW = g.width;
  geo.gridH = g.height;
  geo.levels = maxHeight(g);
  return geo;
}

ote::HexIsoGeometry hexGeo(const ote_editor::MapGrid& g) {
  const ote::Grid grid(ote::GridType::Hex, g.width, g.height, g.flatTop ? ote::HexLayout::Flat : ote::HexLayout::Pointy);
  ote::HexIsoGeometry geo = ote::HexIsoGeometry::make(grid, (int)MapGeometry::kCell, kTileW, kStep);
  geo.levels = maxHeight(g);
  return geo;
}

// A pixmap laid over the parallelogram P0 (its top-left), P1 (top-right), P3 (bottom-left): the top face of a raised square.
void drawSlanted(QPainter& p, const QPixmap& pm, const QPointF& p0, const QPointF& p1, const QPointF& p3) {
  const QPointF a = (p1 - p0) / pm.width(), b = (p3 - p0) / pm.height();
  p.save();
  p.setTransform(QTransform(a.x(), a.y(), b.x(), b.y(), p0.x(), p0.y()), true);
  p.drawPixmap(0, 0, pm);
  p.restore();
}
}  // namespace

IsoPreview::IsoPreview(MapPainter* painter, MapArt* art, QWidget* parent) : QWidget(parent), painter_(painter), art_(art) {
  setObjectName("isoPreview");
  setMinimumSize(160, 120);
  setToolTip(tr("Isometric preview (read-only): the map as the isometric views show it, squares raised to their heights"));
  connect(painter_, &MapPainter::gridChanged, this, [this] {
    markersStale_ = true;
    update();
  });
  connect(art_, &MapArt::artChanged, this, [this] { update(); });
}

std::map<char, IsoPreview::Style> IsoPreview::stylesFor(const ote_editor::MapGrid& g) {
  std::map<std::string, TerrainInfo> infos;
  for (const TerrainInfo& t : painter_->terrains()) infos[t.id] = t;
  std::map<char, Style> out;
  for (const auto& e : g.legend) {
    Style s;
    const auto it = infos.find(e.second);
    s.color = it == infos.end() ? QColor(200, 60, 200) : it->second.color;
    s.art = art_->terrainTile(e.second);
    out[e.first] = s;
  }
  return out;
}

// The scale and corner that fit the whole map into the widget (kept to one place so cellTop() and paintEvent() agree).
void IsoPreview::fit(const ote_editor::MapGrid& g, double* scale, QPointF* origin, int* levels) {
  const bool hex = g.hex;
  double ww, wh;
  if (hex) {
    const ote::HexIsoGeometry geo = hexGeo(g);
    ww = geo.worldW();
    wh = geo.worldH();
  } else {
    const ote::IsoGeometry geo = squareGeo(g);
    ww = geo.worldW();
    wh = geo.worldH();
  }
  *levels = maxHeight(g);
  *scale = std::max(0.05, std::min(4.0, std::min((width() - 2 * kMargin) / ww, (height() - 2 * kMargin) / wh)));
  *origin = QPointF((width() - ww * *scale) / 2, (height() - wh * *scale) / 2);
}

bool IsoPreview::cellTop(int x, int y, QPointF* out) {
  if (!painter_->valid()) return false;
  const ote_editor::MapGrid& g = painter_->grid();
  if (!g.inside(x, y)) return false;
  double scale;
  QPointF origin;
  int levels;
  fit(g, &scale, &origin, &levels);
  float wx, wy;
  if (g.hex) {
    const MapGeometry geom(g.width, g.height, true, g.flatTop);
    const QPointF c = geom.center(x, y);
    hexGeo(g).project((float)c.x(), (float)c.y(), (float)g.heightAt(x, y), wx, wy);
  } else {
    squareGeo(g).project(x + 0.5f, y + 0.5f, (float)g.heightAt(x, y), wx, wy);
  }
  *out = origin + QPointF(wx, wy) * scale;
  return true;
}

void IsoPreview::drawMarkers(QPainter& p, const std::vector<const MapMarker*>& here, const QPointF& mid, double scale) {
  const double u = kTileH * scale;  // about one square's height on screen
  QFont f = p.font();
  f.setPixelSize(std::max(6, (int)(u * 0.6)));
  p.setFont(f);
  for (const MapMarker* m : here) {
    p.setPen(QPen(QColor(0, 0, 0, 200), 1.2));
    p.setBrush(m->color);
    if (m->kind == MapMarker::Kind::PrepSlot) {
      QPolygonF d;
      d << mid + QPointF(0, -u * 0.35) << mid + QPointF(u * 0.7, 0) << mid + QPointF(0, u * 0.35) << mid + QPointF(-u * 0.7, 0);
      p.drawPolygon(d);
    } else if (m->kind == MapMarker::Kind::Structure) {
      p.drawRoundedRect(QRectF(mid.x() - u * 0.5, mid.y() - u * 0.9, u, u), u * 0.1, u * 0.1);
      p.setPen(Qt::white);
      p.drawText(QRectF(mid.x() - u * 0.5, mid.y() - u * 0.9, u, u), Qt::AlignCenter, m->letter);
    } else {  // a unit stands up from its square: a disc on a short post
      p.drawEllipse(mid + QPointF(0, -u * 0.8), u * 0.45, u * 0.45);
      p.setPen(Qt::white);
      p.drawText(QRectF(mid.x() - u * 0.45, mid.y() - u * 1.25, u * 0.9, u * 0.9), Qt::AlignCenter, m->letter);
    }
  }
}

void IsoPreview::drawSquare(QPainter& p, const ote_editor::MapGrid& g, const std::map<char, Style>& styles, double scale, const QPointF& origin) {
  const ote::IsoGeometry geo = squareGeo(g);
  auto pt = [&](float gx, float gy, float e) {
    float wx, wy;
    geo.project(gx, gy, e, wx, wy);
    return origin + QPointF(wx, wy) * scale;
  };
  std::map<int, std::vector<const MapMarker*>> byCell;
  for (const MapMarker& m : markers_)
    if (g.inside(m.x, m.y)) byCell[m.y * g.width + m.x].push_back(&m);
  // Back to front: squares with a smaller x + y are further away.
  for (int s = 0; s <= g.width + g.height - 2; ++s)
    for (int y = std::max(0, s - (g.width - 1)); y <= std::min(g.height - 1, s); ++y) {
      const int x = s - y;
      const int e = g.heightAt(x, y);
      Style st;
      st.color = QColor(200, 60, 200);
      const auto it = styles.find(g.symbolAt(x, y));
      if (it != styles.end()) st = it->second;
      const QPointF A = pt((float)x, (float)y, (float)e), B = pt(x + 1.f, (float)y, (float)e), C = pt(x + 1.f, y + 1.f, (float)e), D = pt((float)x, y + 1.f, (float)e);
      // The two faces turned towards the viewer: the lower-left one (square y + 1 lies there) and the lower-right one (x + 1).
      auto face = [&](const QPointF& a, const QPointF& b, int nx, int ny, const QColor& shade) {
        const int drop = e - (g.inside(nx, ny) ? g.heightAt(nx, ny) : 0);
        if (drop <= 0) return;
        const QPointF down(0, drop * kStep * scale);
        p.setPen(Qt::NoPen);
        p.setBrush(shade);
        p.drawPolygon(QPolygonF() << a << b << b + down << a + down);
      };
      face(D, C, x, y + 1, st.color.darker(150));
      face(C, B, x + 1, y, st.color.darker(190));
      p.setPen(Qt::NoPen);
      p.setBrush(st.color);
      p.drawPolygon(QPolygonF() << A << B << C << D);
      if (!st.art.isNull()) drawSlanted(p, st.art, A, B, D);
      p.setPen(QPen(QColor(0, 0, 0, 60), 1));
      p.setBrush(Qt::NoBrush);
      p.drawPolygon(QPolygonF() << A << B << C << D);
      const auto here = byCell.find(y * g.width + x);
      if (here != byCell.end()) drawMarkers(p, here->second, pt(x + 0.5f, y + 0.5f, (float)e), scale);
    }
}

void IsoPreview::drawHex(QPainter& p, const ote_editor::MapGrid& g, const std::map<char, Style>& styles, double scale, const QPointF& origin) {
  const ote::HexIsoGeometry geo = hexGeo(g);
  const MapGeometry flat(g.width, g.height, true, g.flatTop);  // the top-down hexagons, in the same units as the engine's lattice
  auto pt = [&](const QPointF& f, float e) {
    float wx, wy;
    geo.project((float)f.x(), (float)f.y(), e, wx, wy);
    return origin + QPointF(wx, wy) * scale;
  };
  std::map<int, std::vector<const MapMarker*>> byCell;
  for (const MapMarker& m : markers_)
    if (g.inside(m.x, m.y)) byCell[m.y * g.width + m.x].push_back(&m);
  std::vector<int> order((size_t)g.width * (size_t)g.height);
  for (size_t i = 0; i < order.size(); ++i) order[i] = (int)i;
  // Nearer hexagons (a lower front point on screen) are drawn later; equal rows go left to right (engine rule, HexIsoGeometry::sortKey).
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return geo.sortKey(a % g.width, a / g.width) < geo.sortKey(b % g.width, b / g.width); });
  for (int c : order) {
    const int x = c % g.width, y = c / g.width, e = g.heightAt(x, y);
    Style st;
    st.color = QColor(200, 60, 200);
    const auto it = styles.find(g.symbolAt(x, y));
    if (it != styles.end()) st = it->second;
    const QPolygonF outline = flat.outline(x, y);
    QPolygonF top;
    for (const QPointF& v : outline) top << pt(v, (float)e);
    const QPointF mid = pt(flat.center(x, y), (float)e);
    // Side faces: every edge whose middle lies below the hexagon's middle on screen shows a wall down to the neighbour's height.
    for (int i = 0; i < top.size(); ++i) {
      const QPointF a = top[i], b = top[(i + 1) % top.size()];
      if ((a.y() + b.y()) / 2 < mid.y() + 0.25 * scale * kTileH * 0.5) continue;
      const QPointF fm = (outline[i] + outline[(i + 1) % outline.size()]) / 2;
      const QPointF fc = flat.center(x, y);
      int nx, ny;
      const QPointF out = fm + (fm - fc) * 0.3;  // a point just across the edge
      const bool across = flat.cellAt(out, &nx, &ny);
      const int drop = e - (across ? g.heightAt(nx, ny) : 0);
      if (drop <= 0) continue;
      const QPointF down(0, drop * kStep * scale);
      p.setPen(Qt::NoPen);
      p.setBrush(st.color.darker(i % 2 ? 150 : 190));
      p.drawPolygon(QPolygonF() << a << b << b + down << a + down);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(st.color);
    p.drawPolygon(top);
    if (!st.art.isNull()) {
      // The picture of a hexagon is a box: stretch it over the box of the squashed top face, clipped to the hexagon.
      p.save();
      QPainterPath clip;
      clip.addPolygon(top);
      p.setClipPath(clip, Qt::IntersectClip);
      p.drawPixmap(top.boundingRect().toAlignedRect(), st.art);
      p.restore();
    }
    p.setPen(QPen(QColor(0, 0, 0, 60), 1));
    p.setBrush(Qt::NoBrush);
    p.drawPolygon(top);
    const auto here = byCell.find(c);
    if (here != byCell.end()) drawMarkers(p, here->second, mid, scale);
  }
}

void IsoPreview::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), palette().color(QPalette::Window).darker(118));
  if (!painter_->valid()) return;
  const ote_editor::MapGrid& g = painter_->grid();
  if (g.width < 1 || g.height < 1) return;
  if (markersStale_) {
    markers_ = readMapMarkers(painter_->doc()->root(), painter_->footprints());
    markersStale_ = false;
  }
  double scale;
  QPointF origin;
  int levels;
  fit(g, &scale, &origin, &levels);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, scale < 1.0);  // shrunk pictures look better smoothed; enlarged ones stay crisp
  const std::map<char, Style> styles = stylesFor(g);
  if (g.hex) drawHex(p, g, styles, scale, origin);
  else drawSquare(p, g, styles, scale, origin);
}

}  // namespace ote_native
