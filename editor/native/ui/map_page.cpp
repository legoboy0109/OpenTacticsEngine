#include "map_page.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QIcon>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

#include "map_resize_dialog.h"
#include "project.h"

namespace ote_native {

namespace {
struct ToolDef {
  MapTool tool;
  const char* text;
  const char* tip;
};
const ToolDef kTools[] = {
    {MapTool::Paint, "Paint", "Click or drag to paint the chosen terrain"},
    {MapTool::Rectangle, "Rectangle", "Drag a rectangle and fill it with the chosen terrain"},
    {MapTool::Fill, "Fill", "Fill the connected area of the same terrain with the chosen terrain"},
    {MapTool::Pick, "Pick", "Click a square to choose its terrain (eyedropper); also copies its height into the Height box"},
    {MapTool::Raise, "Raise", "Click or drag to raise the height of squares by 1 (adds a heights list to a map that has none)"},
    {MapTool::Lower, "Lower", "Click or drag to lower the height of squares by 1"},
    {MapTool::SetHeight, "Set height", "Click or drag to set squares to the number in the Height box"},
    {MapTool::Inspect, "Inspect", "Click a unit, structure or deployment slot to open its entry in the Form page"},
    {MapTool::Slots, "Slots", "Deployment slots (preparation screen): click an empty square to add one, drag one to move it, right-click one to remove it"},
    {MapTool::MoveUnit, "Move unit", "Drag a unit to another square"},
};
}  // namespace

MapPage::MapPage(Project* project, JsonDocument* doc, QWidget* parent) : QWidget(parent), project_(project) {
  setObjectName("mapPage");
  painter_ = new MapPainter(project, doc, this);
  art_ = new MapArt(project, this);
  view_ = new MapView(painter_, art_, this);

  tools_ = new QButtonGroup(this);
  auto* bar = new QHBoxLayout;
  for (const ToolDef& t : kTools) {
    auto* b = new QToolButton(this);
    b->setObjectName(QStringLiteral("tool_") + mapToolKey(t.tool));
    b->setText(tr(t.text));
    b->setToolTip(tr(t.tip));
    b->setCheckable(true);
    tools_->addButton(b, (int)t.tool);
    bar->addWidget(b);
  }
  tools_->setExclusive(true);
  connect(tools_, &QButtonGroup::idClicked, this, [this](int id) { setTool((MapTool)id); });
  tools_->button((int)MapTool::Paint)->setChecked(true);

  heightSpin_ = new QSpinBox(this);
  heightSpin_->setObjectName("heightValue");
  heightSpin_->setRange(0, MapPainter::kMaxHeight);
  heightSpin_->setValue(1);
  heightSpin_->setToolTip(tr("The height the Set height tool paints"));
  connect(heightSpin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) { view_->setHeightValue(v); });
  view_->setHeightValue(1);
  bar->addSpacing(8);
  bar->addWidget(new QLabel(tr("Height:"), this));
  bar->addWidget(heightSpin_);
  bar->addSpacing(8);
  brushBox_ = new QComboBox(this);
  brushBox_->setObjectName("brushSize");
  for (int n = 1; n <= MapPainter::kMaxBrush; n += 2) brushBox_->addItem(tr("%1 x %1").arg(n), n);
  brushBox_->setToolTip(tr("How wide Paint, Raise, Lower and Set height work around the square you click: a block of squares, or a hexagon-shaped patch on a hexagon map"));
  connect(brushBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { setBrushSize(brushBox_->currentData().toInt()); });
  bar->addWidget(new QLabel(tr("Brush:"), this));
  bar->addWidget(brushBox_);
  bar->addSpacing(12);

  auto* out = new QToolButton(this);
  out->setObjectName("zoomOut");
  out->setText(QStringLiteral("−"));
  out->setToolTip(tr("Zoom out (or turn the mouse wheel)"));
  zoomLabel_ = new QLabel(this);
  zoomLabel_->setObjectName("zoomLabel");
  zoomLabel_->setMinimumWidth(46);
  zoomLabel_->setAlignment(Qt::AlignCenter);
  auto* in = new QToolButton(this);
  in->setObjectName("zoomIn");
  in->setText(QStringLiteral("+"));
  in->setToolTip(tr("Zoom in (or turn the mouse wheel)"));
  auto* hundred = new QToolButton(this);
  hundred->setObjectName("zoom100");
  hundred->setText(tr("100%"));
  auto* fit = new QToolButton(this);
  fit->setObjectName("zoomFit");
  fit->setText(tr("Fit"));
  fit->setToolTip(tr("Show the whole map"));
  connect(out, &QToolButton::clicked, this, [this] { view_->zoomBy(1 / 1.25); });
  connect(in, &QToolButton::clicked, this, [this] { view_->zoomBy(1.25); });
  connect(hundred, &QToolButton::clicked, this, [this] { view_->setZoom(1.0); });
  connect(fit, &QToolButton::clicked, this, [this] { view_->fitToWindow(); });
  for (QWidget* w : {static_cast<QWidget*>(out), static_cast<QWidget*>(zoomLabel_), static_cast<QWidget*>(in), static_cast<QWidget*>(hundred), static_cast<QWidget*>(fit)})
    bar->addWidget(w);
  bar->addStretch(1);
  auto* resize = new QPushButton(tr("Resize map..."), this);
  resize->setObjectName("resizeButton");
  connect(resize, &QPushButton::clicked, this, [this] { resizeMap(); });
  bar->addWidget(resize);

  auto* overlays = new QHBoxLayout;
  auto makeCheck = [&](const char* name, const QString& text, bool on, const QString& tip) {
    auto* c = new QCheckBox(text, this);
    c->setObjectName(QStringLiteral("overlay_") + name);
    c->setChecked(on);
    c->setToolTip(tip);
    overlays->addWidget(c);
    return c;
  };
  units_ = makeCheck("units", tr("Units"), true, tr("Show the map's units"));
  structures_ = makeCheck("structures", tr("Structures and deployment slots"), true, tr("Show structures and the preparation screen's deployment slots"));
  heights_ = makeCheck("heights", tr("Heights"), true, tr("Shade squares by height and show the number"));
  layer_ = makeCheck("layer", tr("Tile layer picture"), false, tr("Show the hand-painted tile layer instead of the terrain's own look"));
  overlays->addStretch(1);
  connect(units_, &QCheckBox::toggled, this, [this](bool on) { view_->setShowUnits(on); saveOverlay("units", on); });
  connect(structures_, &QCheckBox::toggled, this, [this](bool on) { view_->setShowStructures(on); saveOverlay("structures", on); });
  connect(heights_, &QCheckBox::toggled, this, [this](bool on) { view_->setShowHeights(on); saveOverlay("heights", on); });
  connect(layer_, &QCheckBox::toggled, this, [this](bool on) { view_->setShowLayer(on); saveOverlay("layer", on); });

  palette_ = new QListWidget(this);
  palette_->setObjectName("terrainPalette");
  palette_->setIconSize(QSize(24, 24));
  palette_->setToolTip(tr("Terrain to paint. A terrain the map's legend does not know yet is added to the legend when you paint it."));
  connect(palette_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* cur) {
    if (cur) view_->setTerrain(cur->data(Qt::UserRole).toString().toStdString());
  });
  auto* split = new QSplitter(this);
  split->addWidget(palette_);
  split->addWidget(view_);
  split->setStretchFactor(1, 1);
  split->setSizes({190, 800});
  split->setChildrenCollapsible(false);

  status_ = new QLabel(this);
  status_->setObjectName("mapStatus");
  note_ = new QLabel(this);
  note_->setObjectName("mapNote");
  note_->setWordWrap(true);
  message_ = new QLabel(this);  // why the last slot / unit edit was refused
  message_->setObjectName("mapMessage");
  message_->setWordWrap(true);
  message_->setStyleSheet(QStringLiteral("color: #b00020;"));
  message_->hide();
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(4, 4, 4, 4);
  root->addLayout(bar);
  root->addLayout(overlays);
  root->addWidget(split, 1);
  root->addWidget(note_);
  root->addWidget(message_);
  root->addWidget(status_);

  connect(view_, &MapView::terrainPicked, this, [this](const QString& id, int h) {
    selectTerrain(id);
    heightSpin_->setValue(h);
  });
  connect(view_, &MapView::placementMessage, this, [this](const QString& t) { message_->setText(t); message_->setVisible(!t.isEmpty()); });
  connect(view_, &MapView::cellHovered, this, [this](int x, int y) { updateStatus(x, y); });
  connect(view_, &MapView::zoomChanged, this, [this](double z) {
    updateZoomLabel();
    if (settings_) settings_->setMapZoom(z);
  });
  connect(view_, &MapView::markerActivated, this, [this](int kind, int index) {
    JsonPath p;
    if (kind == (int)MapMarker::Kind::Unit) p = {PathStep::member("units"), PathStep::at((size_t)index)};
    else if (kind == (int)MapMarker::Kind::Structure) p = {PathStep::member("structures"), PathStep::at((size_t)index)};
    else p = {PathStep::member("prep"), PathStep::member("slots"), PathStep::at((size_t)index)};
    emit inspectRequested(p);
  });
  connect(painter_, &MapPainter::gridChanged, this, [this] {
    rebuildPalette();
    updateStatus(-1, -1);
  });
  // A terrain list that changes in data/terrain.json refreshes the palette too.
  if (JsonDocument* t = project->document(QStringLiteral("data/terrain.json"))) connect(t, &JsonDocument::changed, this, [this] { rebuildPalette(); });
  rebuildPalette();
  updateZoomLabel();
  updateStatus(-1, -1);
  heights_->setChecked(painter_->valid() && painter_->grid().hasHeights);
}

void MapPage::setAppSettings(AppSettings* settings) {
  settings_ = settings;
  if (!settings) return;
  setTool(mapToolFromKey(settings->mapTool()));
  setBrushSize(settings->mapBrush());
  view_->setZoom(settings->mapZoom());
  units_->setChecked(settings->mapOverlay("units"));
  structures_->setChecked(settings->mapOverlay("structures"));
  layer_->setChecked(settings->mapOverlay("layer", false));
  // Heights default to "on when the map has any"; a stored choice wins.
  if (settings->mapOverlay("heightsSet", false)) heights_->setChecked(settings->mapOverlay("heights"));
}

void MapPage::saveOverlay(const QString& name, bool on) {
  if (!settings_) return;
  settings_->setMapOverlay(name, on);
  if (name == "heights") settings_->setMapOverlay("heightsSet", true);
}

void MapPage::setBrushSize(int size) {
  painter_->setBrushSize(size);
  const int i = brushBox_->findData(painter_->brushSize());
  if (i >= 0 && i != brushBox_->currentIndex()) brushBox_->setCurrentIndex(i);
  view_->viewport()->update();
  if (settings_) settings_->setMapBrush(painter_->brushSize());
}

void MapPage::setTool(MapTool tool) {
  if (QAbstractButton* b = tools_->button((int)tool)) b->setChecked(true);
  view_->setTool(tool);
  if (tool == MapTool::Slots && !structures_->isChecked()) structures_->setChecked(true);  // slots must be visible to be edited
  if (tool == MapTool::MoveUnit && !units_->isChecked()) units_->setChecked(true);
  if (settings_) settings_->setMapTool(mapToolKey(tool));
}

QString MapPage::selectedTerrain() const { return QString::fromStdString(view_->terrain()); }

void MapPage::selectTerrain(const QString& id) {
  for (int i = 0; i < palette_->count(); ++i)
    if (palette_->item(i)->data(Qt::UserRole).toString() == id) {
      palette_->setCurrentRow(i);
      return;
    }
}

void MapPage::updateZoomLabel() { zoomLabel_->setText(QStringLiteral("%1%").arg((int)std::lround(view_->zoom() * 100))); }

void MapPage::rebuildPalette() {
  if (!painter_->valid()) {
    palette_->clear();
    paletteSignature_.clear();
    return;
  }
  const ote_editor::MapGrid& g = painter_->grid();
  const QList<TerrainInfo> terrains = painter_->terrains();
  QString sig;
  for (const TerrainInfo& t : terrains) sig += QString::fromStdString(t.id) + QLatin1Char(g.symbolFor(t.id) ? g.symbolFor(t.id) : '-') + t.color.name() + QLatin1Char('|');
  sig += g.hasLayer ? QStringLiteral("L") : QString();
  if (sig == paletteSignature_) return;
  paletteSignature_ = sig;
  const QString keep = selectedTerrain();
  {
    const QSignalBlocker block(palette_);  // the selection is restored below, once
    palette_->clear();
    for (const TerrainInfo& t : terrains) {
      const char sym = g.symbolFor(t.id);
      auto* item = new QListWidgetItem(QStringLiteral("%1  [%2]").arg(t.name, sym ? QString(QLatin1Char(sym)) : tr("new")));
      item->setData(Qt::UserRole, QString::fromStdString(t.id));
      QPixmap art = art_->terrainTile(t.id);
      QPixmap icon(24, 24);
      icon.fill(t.color);
      if (!art.isNull()) {
        QPainter p(&icon);
        p.drawPixmap(QRect(0, 0, 24, 24), art);
      }
      item->setIcon(QIcon(icon));
      item->setToolTip(QStringLiteral("%1 (%2)").arg(t.name, QString::fromStdString(t.id)));
      palette_->addItem(item);
    }
  }
  int row = 0;
  for (int i = 0; i < palette_->count(); ++i)
    if (palette_->item(i)->data(Qt::UserRole).toString() == keep) row = i;
  if (palette_->count()) palette_->setCurrentRow(row);
  layer_->setVisible(g.hasLayer && !g.hex);
  note_->setText(g.hasLayer ? tr("This map has a tile layer (a hand-painted picture per square). Painting here changes the terrain only; the layer is kept as it is and moves with a resize.") : QString());
  note_->setVisible(g.hasLayer);
  view_->setTerrain(selectedTerrain().toStdString());
}

void MapPage::updateStatus(int x, int y) {
  if (!painter_->valid()) {
    status_->setText(painter_->error());
    return;
  }
  const ote_editor::MapGrid& g = painter_->grid();
  QString t = tr("%1 x %2 %3 map").arg(g.width).arg(g.height).arg(g.hex ? tr("hexagon") : tr("square"));
  if (g.inside(x, y)) {
    const std::string terrain = g.terrainAt(x, y);
    t += tr("   |   square %1, %2: %3").arg(x).arg(y).arg(terrain.empty() ? tr("(not in the legend)") : QString::fromStdString(terrain));
    if (g.hasHeights) t += tr(", height %1").arg(g.heightAt(x, y));
  }
  status_->setText(t);
}

bool MapPage::resizeMap() {
  if (!painter_->valid()) return false;
  ote_editor::ResizeSpec spec;
  if (resizeChooser_) {
    if (!resizeChooser_(&spec)) return false;
  } else {
    MapResizeDialog dlg(painter_, this);
    if (dlg.exec() != QDialog::Accepted) return false;
    spec = dlg.spec();
  }
  return applyResize(spec);
}

bool MapPage::applyResize(const ote_editor::ResizeSpec& spec) {
  QString err;
  const auto lost = painter_->lostByResize(spec, &err);
  if (!err.isEmpty()) {
    QMessageBox::warning(this, tr("Resize map"), err);
    return false;
  }
  if (!lost.empty()) {
    const QString text = MapResizeDialog::describeLost(lost) + tr("\n\nResize anyway? (Undo brings them back.)");
    const bool yes = cutConfirm_ ? cutConfirm_(text) : QMessageBox::question(this, tr("Resize map"), text) == QMessageBox::Yes;
    if (!yes) return false;
  }
  return painter_->resize(spec, &err);
}

}  // namespace ote_native
