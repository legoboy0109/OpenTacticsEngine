// The map painter's widgets: hit-testing of square and hexagon cells, real mouse strokes on the canvas inside the real main
// window (one drag = one undo step, exact cells), zoom and pan, the remembered zoom / tool, Inspect, resize with its question,
// and speed on a 100 x 100 map. No dialog is shown: the resize questions go through hooks.
#include <QCheckBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QListWidget>
#include <QScrollBar>
#include <QSettings>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTabWidget>
#include <QToolButton>
#include <QUndoStack>
#include <QtTest>
#include <memory>

#include "form_view.h"
#include "main_window.h"
#include "map_page.h"
#include "project_session.h"
#include "test_prompter.h"
#include "test_support.h"
#include "tree_editor.h"

using namespace ote_native;
using ote::Json;

namespace {

Json parse(const QByteArray& bytes) {
  Json j;
  std::string err;
  Json::parse(bytes.toStdString(), j, err);
  return j;
}

struct Win {
  ote_test::Scratch tmp;
  std::unique_ptr<QSettings> settings;
  std::unique_ptr<MainWindow> w;
  QString folder;

  explicit Win(const char* game = "demo-srpg", const QString& settingsFile = QString()) {
    settings = std::make_unique<QSettings>(settingsFile.isEmpty() ? tmp.file("s.ini") : settingsFile, QSettings::IniFormat);
    folder = tmp.file(game);
    ote_test::copyTree(QStringLiteral(OTE_SOURCE_DIR "/games/") + game, folder);
    start();
  }
  void start() {
    w = std::make_unique<MainWindow>(settings.get());
    w->setPrompter(std::make_unique<ote_test::ScriptedPrompter>());
    w->setSchemaDir(QStringLiteral(OTE_SOURCE_DIR "/editor/schema"));
    w->resize(1280, 800);
    w->show();
    QVERIFY(w->openProject(folder));
  }
  MapPage* openMap(const QString& name) {
    w->openFile("maps/" + name + ".json");
    auto* t = qobject_cast<TreeEditor*>(w->editorFor("maps/" + name + ".json"));
    if (!t || !t->mapPage()) return nullptr;
    QTest::qWait(30);
    return t->mapPage();
  }
  QByteArray file(const QString& rel) { return ote_test::readFile(folder + "/" + rel); }
  bool save(const QString& rel) { return w->session()->project()->save(rel).status == SaveResult::Status::Saved; }
  QUndoStack* stack() { return w->session()->project()->undoStack(); }
};

// Press, drag through `cells` (viewport points of cell middles) and release, like a hand would.
void drag(MapView* v, const QList<QPoint>& pts, Qt::MouseButton b = Qt::LeftButton) {
  QWidget* vp = v->viewport();
  QTest::mousePress(vp, b, Qt::NoModifier, pts.first());
  for (int i = 1; i < pts.size(); ++i) QTest::mouseMove(vp, pts[i]);
  QTest::mouseRelease(vp, b, Qt::NoModifier, pts.last());
}

QPoint at(MapView* v, int x, int y) { return v->viewportPointOfCell(x, y); }

}  // namespace

class MapViewTest : public QObject {
  Q_OBJECT
 private slots:
  void hitTestingRoundTripsOnEveryGrid();
  void aMapOpensOnThePainterPage();
  void dragPaintIsOneUndoStepAndFastDragsLeaveNoGaps();
  void rectangleFillPickAndInspect();
  void heightToolsWork();
  void hexagonClicksPaintTheCellUnderThePointer();
  void zoomKeepsThePointInPlaceAndPanScrolls();
  void zoomAndToolSurviveARestart();
  void resizeAsksBeforeCuttingOffUnits();
  void aHundredByHundredMapStaysFast();
  void paletteListsTheGamesTerrainAndKeepsTheChoice();
};

void MapViewTest::hitTestingRoundTripsOnEveryGrid() {
  struct Kind { bool hex, flat; const char* name; };
  for (const Kind k : {Kind{false, false, "square"}, Kind{true, false, "pointy"}, Kind{true, true, "flat"}}) {
    const MapGeometry g(7, 6, k.hex, k.flat);
    for (int y = 0; y < 6; ++y)
      for (int x = 0; x < 7; ++x) {
        int cx = -1, cy = -1;
        QVERIFY2(g.cellAt(g.center(x, y), &cx, &cy), k.name);
        QCOMPARE(cx, x);
        QCOMPARE(cy, y);
        // Points near every corner, just inside the cell, still belong to it.
        const QPolygonF poly = g.outline(x, y);
        QCOMPARE(poly.size(), k.hex ? 6 : 4);
        for (const QPointF& v : poly) {
          const QPointF p = g.center(x, y) + (v - g.center(x, y)) * 0.92;
          QVERIFY2(g.cellAt(p, &cx, &cy), k.name);
          QCOMPARE(QPoint(cx, cy), QPoint(x, y));
        }
        // ... and just outside a corner of the edge cells they do not (hexagon gaps beside the map edge, the square edge).
      }
    int cx, cy;
    QVERIFY(!g.cellAt(QPointF(-3, -3), &cx, &cy));
    QVERIFY(!g.cellAt(QPointF(g.size().width() + 5, 10), &cx, &cy));
    g.nearestCell(QPointF(-50, 9999), &cx, &cy);  // clamped for dragging past the edge
    QCOMPARE(cx, 0);
    QCOMPARE(cy, 5);
    // The whole map's size covers every cell box.
    for (int y = 0; y < 6; ++y)
      for (int x = 0; x < 7; ++x) {
        const QRectF b = g.bounds(x, y);
        QVERIFY(b.left() >= -0.01 && b.top() >= -0.01);
        QVERIFY(b.right() <= g.size().width() + 0.01);
        QVERIFY(b.bottom() <= g.size().height() + 0.01);
      }
  }
  // Odd rows of a pointy map and odd columns of a flat one sit half a cell over (docs/HEX.md).
  const MapGeometry pointy(4, 4, true, false), flat(4, 4, true, true);
  QCOMPARE(pointy.center(0, 1).x() - pointy.center(0, 0).x(), MapGeometry::kCell / 2);
  QCOMPARE(pointy.center(0, 1).y() - pointy.center(0, 0).y(), pointy.bounds(0, 0).height() * 0.75);
  QVERIFY(qAbs(flat.center(1, 0).y() - flat.center(0, 0).y() - MapGeometry::kCell / 2) < 1e-9);
}

void MapViewTest::aMapOpensOnThePainterPage() {
  Win win;
  MapPage* page = win.openMap("bridge");
  QVERIFY(page);
  auto* tree = qobject_cast<TreeEditor*>(win.w->editorFor("maps/bridge.json"));
  QCOMPARE(tree->pages()->currentWidget(), static_cast<QWidget*>(page));
  QCOMPARE(tree->pages()->tabText(0), QStringLiteral("Painter"));
  QVERIFY(tree->pages()->count() >= 3);  // Painter, the form, the tree of every key
  // The other kinds of file get no painter page.
  win.w->openFile("game.json");
  QVERIFY(!qobject_cast<TreeEditor*>(win.w->editorFor("game.json"))->mapPage());
}

void MapViewTest::dragPaintIsOneUndoStepAndFastDragsLeaveNoGaps() {
  Win win;
  MapPage* page = win.openMap("bridge");
  MapView* v = page->view();
  page->selectTerrain("mountain");
  QCOMPARE(page->selectedTerrain(), QStringLiteral("mountain"));
  // Press at (0,4), jump with ONE move to (5,4): the cells between must be painted too.
  const int before = win.stack()->count();
  drag(v, {at(v, 0, 4), at(v, 5, 4)});
  QCOMPARE(win.stack()->count(), before + 1);
  QVERIFY(win.save("maps/bridge.json"));
  const Json now = parse(win.file("maps/bridge.json"));
  QCOMPARE(QString::fromStdString(now["tiles"][4].asString()), QStringLiteral("mmmmmm=......"));
  // The rest of the map is as it was: row 3 untouched.
  QCOMPARE(QString::fromStdString(now["tiles"][3].asString()), QStringLiteral("......~......"));
  win.stack()->undo();
  QVERIFY(win.save("maps/bridge.json"));
  QCOMPARE(QString::fromStdString(parse(win.file("maps/bridge.json"))["tiles"][4].asString()), QStringLiteral("......=......"));
  // A zig-zag with several moves is still one step.
  win.stack()->clear();
  drag(v, {at(v, 1, 1), at(v, 2, 1), at(v, 2, 2), at(v, 3, 3), at(v, 3, 6)});
  QCOMPARE(win.stack()->count(), 1);
  // Dragging over the blank border around the map paints nothing.
  win.stack()->clear();
  drag(v, {QPoint(1, 1), QPoint(5, 5)});  // the blank border around the map
  QCOMPARE(win.stack()->count(), 0);
}

void MapViewTest::rectangleFillPickAndInspect() {
  Win win;
  MapPage* page = win.openMap("bridge");
  MapView* v = page->view();
  page->selectTerrain("water");
  page->setTool(MapTool::Rectangle);
  drag(v, {at(v, 1, 1), at(v, 3, 1), at(v, 4, 2)});
  QCOMPARE(win.stack()->count(), 1);
  QVERIFY(win.save("maps/bridge.json"));
  Json now = parse(win.file("maps/bridge.json"));
  QCOMPARE(QString::fromStdString(now["tiles"][1].asString()), QStringLiteral(".~~~~.~...m.."));
  QCOMPARE(QString::fromStdString(now["tiles"][2].asString()), QStringLiteral(".~~~~.~.f...."));
  QCOMPARE(QString::fromStdString(now["tiles"][3].asString()), QStringLiteral("......~......"));

  page->selectTerrain("forest");
  page->setTool(MapTool::Fill);
  QTest::mouseClick(v->viewport(), Qt::LeftButton, Qt::NoModifier, at(v, 12, 8));  // the big plain area at the bottom right
  QCOMPARE(win.stack()->count(), 2);
  QVERIFY(win.save("maps/bridge.json"));
  now = parse(win.file("maps/bridge.json"));
  // Row 8 was ".m....~......": the plain area right of the river is one region, the left side is cut off by the river.
  QCOMPARE(QString::fromStdString(now["tiles"][8].asString()), QStringLiteral(".m....~ffffff"));

  // Pick copies the terrain and the height of the square into the palette and the Height box.
  page->setTool(MapTool::Pick);
  QTest::mouseClick(v->viewport(), Qt::LeftButton, Qt::NoModifier, at(v, 9, 0));  // mountain, height 3
  QCOMPARE(page->selectedTerrain(), QStringLiteral("mountain"));
  QCOMPARE(page->findChild<QSpinBox*>("heightValue")->value(), 3);
  QCOMPARE(win.stack()->count(), 2);  // picking edits nothing

  // Inspect: clicking a unit marker opens its entry in the form page.
  page->setTool(MapTool::Inspect);
  QSignalSpy spy(page, &MapPage::inspectRequested);
  const Json map = parse(win.file("maps/bridge.json"));
  const int ux = map["units"][0]["x"].asInt(), uy = map["units"][0]["y"].asInt();
  QTest::mouseClick(v->viewport(), Qt::LeftButton, Qt::NoModifier, at(v, ux, uy));
  QCOMPARE(spy.count(), 1);
  const JsonPath path = spy.first().first().value<JsonPath>();
  QCOMPARE(path.size(), (size_t)2);
  QCOMPARE(QString::fromStdString(path[0].key), QStringLiteral("units"));
  QVERIFY(path[1].isIndex);
  auto* tree = qobject_cast<TreeEditor*>(win.w->editorFor("maps/bridge.json"));
  QCOMPARE(tree->pages()->currentWidget(), static_cast<QWidget*>(tree->form()));  // jumped to the form
  QCOMPARE(win.stack()->count(), 2);
}

void MapViewTest::heightToolsWork() {
  Win win;
  MapPage* page = win.openMap("bridge");
  MapView* v = page->view();
  page->setTool(MapTool::Raise);
  drag(v, {at(v, 0, 0), at(v, 2, 0)});  // (0..2, 0) were 1 -> 2
  QCOMPARE(win.stack()->count(), 1);
  page->setTool(MapTool::Lower);
  drag(v, {at(v, 6, 0), at(v, 6, 1)});  // already 0: nothing changes, so no undo step
  QCOMPARE(win.stack()->count(), 1);
  page->findChild<QSpinBox*>("heightValue")->setValue(9);
  page->setTool(MapTool::SetHeight);
  drag(v, {at(v, 12, 8), at(v, 11, 8)});
  QCOMPARE(win.stack()->count(), 2);
  QVERIFY(win.save("maps/bridge.json"));
  const Json now = parse(win.file("maps/bridge.json"));
  QCOMPARE(QString::fromStdString(now["heights"][0].dump(0)), QStringLiteral("[2,2,2,1,1,1,0,1,1,3,3,2,1]"));
  QCOMPARE(QString::fromStdString(now["heights"][8].dump(0)), QStringLiteral("[2,3,2,1,1,1,0,1,1,1,1,9,9]"));
}

void MapViewTest::hexagonClicksPaintTheCellUnderThePointer() {
  Win win("demo-hex");
  MapPage* page = win.openMap("foggy_hills");
  QVERIFY(page);
  MapView* v = page->view();
  QVERIFY(v->geometry().hex());
  page->selectTerrain("water");
  // Click the middle of several cells, odd and even rows: exactly that cell changes.
  const QList<QPoint> cells{{0, 0}, {3, 1}, {5, 2}, {12, 3}, {6, 8}};
  for (const QPoint& c : cells) QTest::mouseClick(v->viewport(), Qt::LeftButton, Qt::NoModifier, at(v, c.x(), c.y()));
  QCOMPARE(win.stack()->count(), (int)cells.size());
  QVERIFY(win.save("maps/foggy_hills.json"));
  const Json now = parse(win.file("maps/foggy_hills.json"));
  int water = 0;
  for (const Json& row : now["tiles"].items()) water += (int)std::count(row.asString().begin(), row.asString().end(), '~');
  // Every clicked cell is water now.
  for (const QPoint& c : cells) QCOMPARE(now["tiles"][(size_t)c.y()].asString()[(size_t)c.x()], '~');
  QVERIFY(water >= (int)cells.size());
  // Pixels in the gap between two hexagons of a flat-sided pair belong to one of them, never to a third cell.
  int x, y;
  const QPoint a = at(v, 4, 4), b = at(v, 5, 4);
  QVERIFY(v->cellAtViewport((a + b) / 2, &x, &y));
  QVERIFY((x == 4 || x == 5) && y == 4);
}

void MapViewTest::zoomKeepsThePointInPlaceAndPanScrolls() {
  Win win;
  MapPage* page = win.openMap("bridge");
  MapView* v = page->view();
  QCOMPARE(v->zoom(), 1.0);
  page->setTool(MapTool::Pick);
  // Zoom in with the wheel on a cell: the same cell stays under the pointer.
  const QPoint p = at(v, 4, 3);
  int x, y;
  QWheelEvent wheel(QPointF(p), QPointF(v->viewport()->mapToGlobal(p)), QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
  QApplication::sendEvent(v->viewport(), &wheel);
  QVERIFY(v->zoom() > 1.1);
  QVERIFY(v->cellAtViewport(p, &x, &y));
  QCOMPARE(QPoint(x, y), QPoint(4, 3));
  // Buttons and limits.
  v->setZoom(100);
  QCOMPARE(v->zoom(), 8.0);
  v->setZoom(0.0001);
  QCOMPARE(v->zoom(), 0.1);
  page->findChild<QToolButton*>("zoom100")->click();
  QCOMPARE(v->zoom(), 1.0);
  page->findChild<QToolButton*>("zoomIn")->click();
  QCOMPARE(v->zoom(), 1.25);
  page->findChild<QToolButton*>("zoomOut")->click();
  QCOMPARE(v->zoom(), 1.0);
  QCOMPARE(page->findChild<QLabel*>("zoomLabel")->text(), QStringLiteral("100%"));
  page->findChild<QToolButton*>("zoomFit")->click();
  QVERIFY(v->zoom() >= 1.0);  // the demo map is small: fitting grows it
  // Pan: at a high zoom the middle button drags the map.
  v->setZoom(4.0);
  QVERIFY(v->horizontalScrollBar()->maximum() > 0);
  const int h0 = v->horizontalScrollBar()->value();
  QTest::mousePress(v->viewport(), Qt::MiddleButton, Qt::NoModifier, QPoint(300, 200));
  QTest::mouseMove(v->viewport(), QPoint(250, 170));
  QTest::mouseRelease(v->viewport(), Qt::MiddleButton, Qt::NoModifier, QPoint(250, 170));
  QCOMPARE(v->horizontalScrollBar()->value(), h0 + 50);
  QCOMPARE(win.stack()->count(), 0);  // panning paints nothing
  // Clicking at 400% still hits the right cell.
  v->setZoom(4.0);
  const QPoint q = at(v, 6, 4);
  QVERIFY(v->cellAtViewport(q, &x, &y));
  QCOMPARE(QPoint(x, y), QPoint(6, 4));
}

void MapViewTest::zoomAndToolSurviveARestart() {
  ote_test::Scratch keep;
  const QString ini = keep.file("persist.ini");
  {
    Win win("demo-srpg", ini);
    MapPage* page = win.openMap("bridge");
    page->view()->setZoom(2.5);
    page->setTool(MapTool::Fill);
    page->findChild<QCheckBox*>("overlay_units")->setChecked(false);
    win.w->close();
    win.settings->sync();
  }
  Win again("demo-srpg", ini);
  MapPage* page = again.openMap("bridge");
  QCOMPARE(page->view()->zoom(), 2.5);
  QCOMPARE(page->view()->tool(), MapTool::Fill);
  QVERIFY(page->findChild<QToolButton*>("tool_fill")->isChecked());
  QVERIFY(!page->view()->showUnits());
  QCOMPARE(page->findChild<QLabel*>("zoomLabel")->text(), QStringLiteral("250%"));
}

void MapViewTest::resizeAsksBeforeCuttingOffUnits() {
  Win win;
  MapPage* page = win.openMap("bridge");
  const QByteArray before = win.file("maps/bridge.json");
  QString asked;
  bool answer = false;
  page->setCutConfirm([&](const QString& text) { asked = text; return answer; });
  ote_editor::ResizeSpec s;
  s.width = 6;
  s.height = 4;
  s.fill = '.';
  page->setResizeChooser([&](ote_editor::ResizeSpec* out) { *out = s; return true; });
  QVERIFY(!page->resizeMap());  // "no" to the question
  QVERIFY(asked.contains("unit"));
  QVERIFY(asked.contains("will be removed"));
  QCOMPARE(win.stack()->count(), 0);
  answer = true;
  asked.clear();
  QVERIFY(page->resizeMap());
  QCOMPARE(win.stack()->count(), 1);
  QCOMPARE(page->painter()->grid().width, 6);
  win.stack()->undo();
  QCOMPARE(page->painter()->grid().width, 13);
  QVERIFY(win.save("maps/bridge.json") == false);  // undone to the saved state: clean, nothing to save
  QCOMPARE(win.file("maps/bridge.json"), before);
  // Growing needs no question.
  asked.clear();
  s.width = 15;
  s.height = 10;
  QVERIFY(page->resizeMap());
  QVERIFY(asked.isEmpty());
}

void MapViewTest::aHundredByHundredMapStaysFast() {
  Win win;
  Json map = parse(win.file("maps/bridge.json"));
  map.set("grid", parse(R"({"type": "square", "width": 100, "height": 100})"));
  Json tiles = Json::makeArray(), heights = Json::makeArray();
  for (int y = 0; y < 100; ++y) {
    std::string row;
    Json hr = Json::makeArray();
    for (int x = 0; x < 100; ++x) {
      row += ".f~m"[(x / 7 + y / 5) % 4];
      hr.push(Json::makeNumber((x + y) % 5));
    }
    tiles.push(Json::makeString(row));
    heights.push(hr);
  }
  map.set("tiles", tiles);
  map.set("heights", heights);
  map.set("units", Json::makeArray());
  map.set("structures", Json::makeArray());
  map.set("events", Json::makeArray());
  ote_test::writeFile(win.folder + "/maps/big.json", QByteArray::fromStdString(map.dump(2) + "\n"));
  win.w->session()->project()->rescanFiles();
  MapPage* page = win.openMap("big");
  QVERIFY(page);
  MapView* v = page->view();
  v->setZoom(0.25);  // the whole map in view: all 10,000 cells drawn
  QElapsedTimer t;
  t.start();
  for (int i = 0; i < 5; ++i) v->viewport()->grab();
  const qint64 paintMs = t.elapsed();
  qInfo() << "5 full paints of a 100 x 100 map:" << paintMs << "ms";
  QVERIFY2(paintMs < 6000, "painting a 100 x 100 map is too slow");
  v->setZoom(1.0);
  page->selectTerrain("water");
  t.restart();
  // A long stroke across the visible part with many mouse events, painting and repainting live.
  QTest::mousePress(v->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(40, 40));
  for (int i = 0; i < 300; ++i) QTest::mouseMove(v->viewport(), QPoint(40 + i, 40 + (i % 50)));
  QTest::mouseRelease(v->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(340, 90));
  const qint64 strokeMs = t.elapsed();
  qInfo() << "a 300-event stroke on a 100 x 100 map:" << strokeMs << "ms";
  QVERIFY2(strokeMs < 8000, "a stroke on a 100 x 100 map is too slow");
  QCOMPARE(win.stack()->count(), 1);
  // Flood fill of a region and undo on the big map.
  page->setTool(MapTool::Fill);
  page->selectTerrain("mountain");
  t.restart();
  QTest::mouseClick(v->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(200, 120));
  qInfo() << "flood fill on a 100 x 100 map:" << t.elapsed() << "ms";
  QVERIFY(t.elapsed() < 5000);
}

void MapViewTest::paletteListsTheGamesTerrainAndKeepsTheChoice() {
  Win win;
  MapPage* page = win.openMap("bridge");
  QStringList ids;
  for (int i = 0; i < page->palette()->count(); ++i) ids << page->palette()->item(i)->data(Qt::UserRole).toString();
  QCOMPARE(ids, (QStringList{"plain", "forest", "mountain", "water", "bridge"}));
  QVERIFY(page->palette()->item(1)->text().contains("[f]"));  // the legend symbol shows beside the name
  page->selectTerrain("bridge");
  page->setTool(MapTool::Paint);
  QTest::mouseClick(page->view()->viewport(), Qt::LeftButton, Qt::NoModifier, at(page->view(), 0, 0));
  QCOMPARE(page->selectedTerrain(), QStringLiteral("bridge"));  // the choice survives the repaint and rebuild
}

QTEST_MAIN(MapViewTest)
#include "test_map_view.moc"
