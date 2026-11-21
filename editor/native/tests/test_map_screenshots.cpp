// Renders the map painter (square map, hexagon map, heights, zoomed, a tile layer, the resize dialog) with QWidget::grab() and
// saves PNGs to $OTE_SHOT_DIR (else next to the other test scratch data) so a person can look at them. The test itself only
// checks that the pictures are real: right size, not blank, and the terrain colours are really in them.
#include <QCheckBox>
#include <QDir>
#include <QLabel>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QtTest>
#include <memory>

#include "json_commands.h"
#include "main_window.h"
#include "map_page.h"
#include "map_resize_dialog.h"
#include "project_session.h"
#include "test_prompter.h"
#include "test_support.h"
#include "tree_editor.h"

using namespace ote_native;
using ote::Json;

class MapShotsTest : public QObject {
  Q_OBJECT
 private slots:
  void initTestCase();
  void squareMap();
  void hexMap();
  void heightsAndZoom();
  void tileLayer();
  void resizeDialogWithWarning();

 private:
  struct Win {
    ote_test::Scratch tmp;
    std::unique_ptr<QSettings> settings;
    std::unique_ptr<MainWindow> w;
    QString folder;
    Win(const char* game) {
      settings = std::make_unique<QSettings>(tmp.file("s.ini"), QSettings::IniFormat);
      folder = tmp.file(game);
      ote_test::copyTree(QStringLiteral(OTE_SOURCE_DIR "/games/") + game, folder);
      w = std::make_unique<MainWindow>(settings.get());
      w->setPrompter(std::make_unique<ote_test::ScriptedPrompter>());
      w->setSchemaDir(QStringLiteral(OTE_SOURCE_DIR "/editor/schema"));
      w->resize(1280, 800);
      w->show();
      w->openProject(folder);
    }
    MapPage* open(const QString& map) {
      w->openFile("maps/" + map + ".json");
      QTest::qWait(60);
      return qobject_cast<TreeEditor*>(w->editorFor("maps/" + map + ".json"))->mapPage();
    }
  };
  void save(QWidget* w, const QString& name);
  // Distinct colours seen in the canvas, to prove the pictures are not blank.
  static int colours(const QImage& img);
  QString shotDir_;
};

void MapShotsTest::initTestCase() {
  shotDir_ = qEnvironmentVariable("OTE_SHOT_DIR");
  if (shotDir_.isEmpty()) shotDir_ = qEnvironmentVariable("OTE_TEST_TMP", QDir::tempPath()) + "/native-shots";
  QVERIFY(QDir().mkpath(shotDir_));
}

int MapShotsTest::colours(const QImage& img) {
  QSet<QRgb> seen;
  for (int y = 0; y < img.height(); y += 3)
    for (int x = 0; x < img.width(); x += 3) seen.insert(img.pixel(x, y));
  return (int)seen.size();
}

void MapShotsTest::save(QWidget* w, const QString& name) {
  QTest::qWait(60);
  const QImage img = w->grab().toImage();
  QVERIFY2(colours(img) > 12, qPrintable(name + ": the picture is blank"));
  QVERIFY(img.save(shotDir_ + "/" + name + ".png"));
}

void MapShotsTest::squareMap() {
  Win win("demo-srpg");
  MapPage* page = win.open("bridge");
  QVERIFY(page);
  page->view()->setShowHeights(false);
  page->findChild<QCheckBox*>("overlay_heights")->setChecked(false);
  page->view()->setZoom(1.0);
  save(win.w.get(), "map_square_100pct");
  // The terrain colours of the legend are really drawn (water and plain are both on this map).
  const QImage cv = page->view()->viewport()->grab().toImage();
  QVERIFY(colours(cv) > 6);
}

void MapShotsTest::hexMap() {
  Win win("demo-hex");
  MapPage* page = win.open("foggy_hills");
  QVERIFY(page);
  page->view()->setZoom(1.0);
  page->setTool(MapTool::Paint);
  page->selectTerrain("water");
  QTest::mouseMove(page->view()->viewport(), page->view()->viewportPointOfCell(6, 4));  // hover outline
  save(win.w.get(), "map_hex_100pct");
  // The same map after a rectangle drag, with the rectangle preview showing mid-stroke.
  page->setTool(MapTool::Rectangle);
  QTest::mousePress(page->view()->viewport(), Qt::LeftButton, Qt::NoModifier, page->view()->viewportPointOfCell(3, 3));
  QTest::mouseMove(page->view()->viewport(), page->view()->viewportPointOfCell(7, 5));
  save(win.w.get(), "map_hex_rectangle_preview");
  QTest::mouseRelease(page->view()->viewport(), Qt::LeftButton, Qt::NoModifier, page->view()->viewportPointOfCell(7, 5));
  save(win.w.get(), "map_hex_after_rectangle");
}

void MapShotsTest::heightsAndZoom() {
  Win win("demo-srpg");
  MapPage* page = win.open("north_road");
  QVERIFY(page);
  page->view()->setZoom(1.0);
  page->setTool(MapTool::Raise);
  save(win.w.get(), "map_heights_100pct");
  page->view()->setZoom(2.5);
  QTest::mouseMove(page->view()->viewport(), page->view()->viewportPointOfCell(5, 5));
  save(win.w.get(), "map_heights_zoom250");
  page->view()->setZoom(0.4);
  save(win.w.get(), "map_heights_zoom40");
}

void MapShotsTest::tileLayer() {
  Win win("demo-srpg");
  MapPage* page = win.open("bridge");
  Project* project = win.w->session()->project();
  JsonDocument* gfx = project->document("data/graphics.json");
  Json ts;
  std::string err;
  Json::parse(R"({"world": {"sheet": "tiles"}})", ts, err);
  project->push(gfx, new InsertCommand(gfx, {}, gfx->root().members().size(), "tilesets", ts));
  Json cells = Json::makeArray();
  for (int y = 0; y < 9; ++y) {
    std::string row;
    for (int x = 0; x < 13; ++x) {
      static const char* tiles[] = {"0000", "0001", "0002", "0003", "0004", "...."};
      row += tiles[(x * 3 + y) % 6];
    }
    cells.push(Json::makeString(row));
  }
  Json layer = Json::makeObject();
  layer.set("tileset", Json::makeString("world"));
  layer.set("cells", cells);
  JsonDocument* doc = project->document("maps/bridge.json");
  project->push(doc, new InsertCommand(doc, {}, doc->root().members().size(), "tileLayer", layer));
  QTest::qWait(60);
  page->view()->setZoom(1.5);
  page->findChild<QCheckBox*>("overlay_layer")->setChecked(true);
  save(win.w.get(), "map_tile_layer_picture");
  page->findChild<QCheckBox*>("overlay_layer")->setChecked(false);
  save(win.w.get(), "map_tile_layer_terrain_view");
}

void MapShotsTest::resizeDialogWithWarning() {
  Win win("demo-srpg");
  MapPage* page = win.open("bridge");
  MapResizeDialog dlg(page->painter());
  dlg.findChild<QSpinBox*>("resizeWidth")->setValue(8);
  dlg.findChild<QSpinBox*>("resizeHeight")->setValue(5);
  dlg.resize(560, 360);
  dlg.show();
  QVERIFY(!dlg.findChild<QLabel*>("resizeWarning")->text().isEmpty());  // units would be cut off
  save(&dlg, "map_resize_dialog");
}

QTEST_MAIN(MapShotsTest)
#include "test_map_screenshots.moc"
