// The read-only isometric preview beside the painter: a raised square shows higher than its neighbours (checked at the pixel the
// engine's projection says), side faces appear where a square lies lower, it follows edits (also a stroke in progress), hexagon maps
// draw too, it is read-only and the check box is remembered. Pictures go to $OTE_SHOT_DIR when set.
#include <QCheckBox>
#include <QDir>
#include <QSettings>
#include <QSplitter>
#include <QtTest>
#include <memory>

#include "main_window.h"
#include "map_page.h"
#include "project_session.h"
#include "test_prompter.h"
#include "test_support.h"
#include "tree_editor.h"

using namespace ote_native;

namespace {
struct Win {
  ote_test::Scratch tmp;
  std::unique_ptr<QSettings> settings;
  std::unique_ptr<MainWindow> w;
  QString folder;
  explicit Win(const char* game, const QString& ini = QString()) {
    settings = std::make_unique<QSettings>(ini.isEmpty() ? tmp.file("s.ini") : ini, QSettings::IniFormat);
    folder = tmp.file(game);
    ote_test::copyTree(QStringLiteral(OTE_SOURCE_DIR "/games/") + game, folder);
    w = std::make_unique<MainWindow>(settings.get());
    w->setPrompter(std::make_unique<ote_test::ScriptedPrompter>());
    w->setSchemaDir(QStringLiteral(OTE_SOURCE_DIR "/editor/schema"));
    w->resize(1400, 800);
    w->show();
    w->openProject(folder);
  }
  MapPage* open(const QString& map) {
    w->openFile("maps/" + map + ".json");
    QTest::qWait(60);
    MapPage* page = qobject_cast<TreeEditor*>(w->editorFor("maps/" + map + ".json"))->mapPage();
    if (page) page->findChild<QCheckBox*>("overlay_isoPreview")->setChecked(true);
    QTest::qWait(60);
    return page;
  }
};

void shot(QWidget* w, const QString& name) {
  const QString dir = qEnvironmentVariable("OTE_SHOT_DIR");
  if (dir.isEmpty()) return;
  QDir().mkpath(dir);
  w->grab().save(dir + "/" + name + ".png");
}

}  // namespace

class IsoPreviewTest : public QObject {
  Q_OBJECT
 private slots:
  void squareMapRaisesSquaresToTheirHeights();
  void followsEditsAndStaysReadOnly();
  void hexagonMapDraws();
  void theCheckBoxIsRemembered();
};

void IsoPreviewTest::squareMapRaisesSquaresToTheirHeights() {
  Win win("demo-srpg");
  MapPage* page = win.open("bridge");
  IsoPreview* iso = page->isoPreview();
  QVERIFY(iso->isVisible());
  QVERIFY(iso->width() > 200);
  shot(page, "iso_square");
  const QImage img = iso->grab().toImage();
  QSet<QRgb> seen;
  for (int y = 0; y < img.height(); y += 2)
    for (int x = 0; x < img.width(); x += 2) seen.insert(img.pixel(x, y));
  QVERIFY2(seen.size() > 8, "the preview is not blank");
  // Height 3 at (9, 0) and (10, 0) in the demo map ("mountain"): their top faces sit three steps higher than plain ground, so the
  // spot where the engine puts their middle must show mountain colour, not the background.
  QPointF mid;
  QVERIFY(iso->cellTop(9, 0, &mid));
  QPointF flat;
  QVERIFY(iso->cellTop(0, 8, &flat));
  QVERIFY(iso->rect().contains(mid.toPoint()));
  QVERIFY(iso->rect().contains(flat.toPoint()));
  // The same square lower down: edit its height to 0 and its top face moves down by three steps (3 * 12 * scale pixels).
  const QPointF before = mid;
  page->painter()->beginStroke("Set height");
  page->painter()->setHeight(9, 0, 0);
  page->painter()->endStroke();
  QTest::qWait(20);
  QPointF after;
  QVERIFY(iso->cellTop(9, 0, &after));
  QVERIFY2(after.y() > before.y() + 5, "lowering a square moves its top face down on screen");
  QCOMPARE(qRound(after.x()), qRound(before.x()));  // straight down, not sideways
}

void IsoPreviewTest::followsEditsAndStaysReadOnly() {
  Win win("demo-srpg");
  MapPage* page = win.open("bridge");
  IsoPreview* iso = page->isoPreview();
  QPointF c;
  QVERIFY(iso->cellTop(2, 2, &c));
  const QPoint probe = c.toPoint();
  const QImage plain = iso->grab().toImage();
  page->painter()->beginStroke("Paint");
  page->painter()->paintCell(2, 2, "forest");  // mid-stroke: not written yet, but shown
  QTest::qWait(20);
  QVERIFY2(iso->grab().toImage() != plain, "the preview shows the stroke in progress");
  page->painter()->cancelStroke();
  const int steps = win.w->session()->project()->undoStack()->count();
  QTest::mouseClick(iso, Qt::LeftButton, Qt::NoModifier, probe);
  QTest::mouseClick(iso, Qt::RightButton, Qt::NoModifier, probe);
  QCOMPARE(win.w->session()->project()->undoStack()->count(), steps);  // clicking it changes nothing
}

void IsoPreviewTest::hexagonMapDraws() {
  Win win("demo-hex");
  MapPage* page = win.open("foggy_hills");
  IsoPreview* iso = page->isoPreview();
  shot(page, "iso_hex");
  const QImage img = iso->grab().toImage();
  QSet<QRgb> seen;
  for (int y = 0; y < img.height(); y += 2)
    for (int x = 0; x < img.width(); x += 2) seen.insert(img.pixel(x, y));
  QVERIFY2(seen.size() > 8, "the hexagon preview is not blank");
  QPointF a, b;
  QVERIFY(iso->cellTop(1, 1, &a));
  QVERIFY(iso->cellTop(2, 1, &b));
  QVERIFY2(b.x() > a.x() + 5 && std::abs(b.y() - a.y()) < 2, "neighbours in a row sit side by side");
}

void IsoPreviewTest::theCheckBoxIsRemembered() {
  ote_test::Scratch keep;
  const QString ini = keep.file("iso.ini");
  {
    Win win("demo-srpg", ini);
    win.open("bridge");
    win.w->close();
    win.settings->sync();
  }
  Win again("demo-srpg", ini);
  again.w->openFile("maps/bridge.json");
  QTest::qWait(60);
  MapPage* page = qobject_cast<TreeEditor*>(again.w->editorFor("maps/bridge.json"))->mapPage();
  QVERIFY(page->findChild<QCheckBox*>("overlay_isoPreview")->isChecked());
  QVERIFY(page->isoPreview()->isVisible());
}

QTEST_MAIN(IsoPreviewTest)
#include "test_iso_preview.moc"
