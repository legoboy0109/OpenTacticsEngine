// Placing things on the painter's canvas: deployment slots (add, move, remove; renumbering of "required" slot numbers; the
// "limit" kept valid), moving units, and the same through real mouse events on the canvas (Slots / Move unit tools). Every action
// is one undo step and the saved map must still pass the engine's own project check.
#include <QLabel>
#include <QtTest>
#include <filesystem>
#include <memory>

#include "map_page.h"
#include "map_placement.h"
#include "project.h"
#include "test_support.h"
#include "validate.h"

using namespace ote_native;
using ote::Json;

namespace {

struct Fixture {
  ote_test::Scratch tmp;
  QString dir;
  std::unique_ptr<Project> project;
  JsonDocument* doc = nullptr;

  Fixture(const char* fixture, const char* map) {
    dir = tmp.file(fixture);
    ote_test::copyTree(QStringLiteral(OTE_SOURCE_DIR "/tests/fixtures/") + fixture, dir);
    project = std::make_unique<Project>(dir);
    doc = project->document(QStringLiteral("maps/%1.json").arg(map));
  }
  const Json& root() const { return doc->root(); }
  int steps() const { return project->undoStack()->count(); }
  // The project check of the saved files: no problem may mention this map.
  QStringList problems() {
    project->save(doc->relPath());
    QStringList out;
    for (const auto& p : ote_editor::validateProject(std::filesystem::path(dir.toStdString())))
      out << QString::fromStdString(p.file + ": " + p.message);
    return out;
  }
  QByteArray original() const { return doc->serialize(); }
};

// The slots as "x,y x,y ..." for short comparisons.
QString slotList(const Json& map) {
  QStringList out;
  for (size_t i = 0; i < map["prep"]["slots"].size(); ++i)
    out << QStringLiteral("%1,%2").arg(map["prep"]["slots"][i]["x"].asInt()).arg(map["prep"]["slots"][i]["y"].asInt());
  return out.join(' ');
}

}  // namespace

class MapPlacementTest : public QObject {
  Q_OBJECT
 private slots:
  void slotsAddMoveRemoveAreOneUndoStepEach();
  void removingASlotKeepsRequiredAndLimitValid();
  void aMapWithoutPrepGetsOneWithItsFirstSlot();
  void unitsMoveButNeverOntoAnotherUnit();
  void theCanvasToolsDoTheSame();
};

void MapPlacementTest::slotsAddMoveRemoveAreOneUndoStepEach() {
  Fixture f("srpg-prep", "hilltop");
  const QByteArray start = f.original();
  MapPlacement pl(f.project.get(), f.doc);
  QString err;
  QCOMPARE(slotList(f.root()), QStringLiteral("0,0 0,1 0,2 0,3"));
  QVERIFY(pl.addSlot(1, 4, 8, 6, &err));
  QCOMPARE(slotList(f.root()), QStringLiteral("0,0 0,1 0,2 0,3 1,4"));
  QCOMPARE(f.steps(), 1);
  QVERIFY(!pl.addSlot(1, 4, 8, 6, &err));  // taken
  QVERIFY(!err.isEmpty());
  QVERIFY(!pl.addSlot(8, 0, 8, 6, &err));  // outside the map
  QCOMPARE(f.steps(), 1);
  QVERIFY(pl.moveSlot(4, 5, 5, 8, 6, &err));
  QCOMPARE(slotList(f.root()), QStringLiteral("0,0 0,1 0,2 0,3 5,5"));
  QCOMPARE(f.steps(), 2);
  err.clear();
  QVERIFY(!pl.moveSlot(4, 0, 0, 8, 6, &err));  // onto another slot
  QVERIFY(!err.isEmpty());
  QVERIFY(!pl.moveSlot(4, 5, 5, 8, 6, &err));  // same square: nothing to write
  QCOMPARE(f.steps(), 2);
  QVERIFY(f.problems().isEmpty());
  QVERIFY(pl.removeSlot(0, &err));
  QCOMPARE(slotList(f.root()), QStringLiteral("0,1 0,2 0,3 5,5"));
  QCOMPARE(f.steps(), 3);
  f.project->undoStack()->undo();
  f.project->undoStack()->undo();
  f.project->undoStack()->undo();
  QCOMPARE(f.original(), start);  // undo restores every byte
}

void MapPlacementTest::removingASlotKeepsRequiredAndLimitValid() {
  Fixture f("srpg-prep", "orchard");  // slots (0,1) (0,2) (0,3) (1,0); required sera on slot 1; limit 3
  MapPlacement pl(f.project.get(), f.doc);
  QString err;
  QVERIFY(pl.addSlot(5, 5, 8, 6, &err));
  // Make a second required entry that names slot 3, so the renumbering shows.
  Json req = Json::makeArray();
  Json a = Json::makeObject();
  a.set("character", Json::makeString("sera"));
  a.set("slot", Json::makeNumber(1));
  Json b = Json::makeObject();
  b.set("character", Json::makeString("ilsa"));
  b.set("slot", Json::makeNumber(3));
  req.push(a);
  req.push(b);
  f.doc->applySet({PathStep::member("prep"), PathStep::member("required")}, req);
  QVERIFY(pl.removeSlot(0, &err));
  const Json& r = f.root()["prep"]["required"];
  QVERIFY(!r[0].has("slot"));                 // sera's own slot is gone: still required, no fixed square
  QCOMPARE(r[0]["character"].asString(), std::string("sera"));
  QCOMPARE(r[1]["slot"].asInt(), 2);          // slot 3 is slot 2 now
  QCOMPARE(slotList(f.root()), QStringLiteral("0,2 0,3 1,0 5,5"));
  QCOMPARE(f.root()["prep"]["limit"].asInt(), 3);
  QVERIFY(pl.removeSlot(3, &err));
  QVERIFY(pl.removeSlot(2, &err));
  QCOMPARE(f.root()["prep"]["limit"].asInt(), 2);  // never more than the slots left
  // The last slot stays: the loader wants at least one.
  while (f.root()["prep"]["slots"].size() > 1) QVERIFY(pl.removeSlot(0, &err));
  QVERIFY(!pl.removeSlot(0, &err));
  QVERIFY(err.contains("at least one"));
}

void MapPlacementTest::aMapWithoutPrepGetsOneWithItsFirstSlot() {
  Fixture f("srpg-prep", "meadow");
  QVERIFY(!f.root().has("prep"));
  MapPlacement pl(f.project.get(), f.doc);
  QString err;
  QVERIFY(pl.addSlot(0, 0, f.root()["grid"]["width"].asInt(), f.root()["grid"]["height"].asInt(), &err));
  QCOMPARE(slotList(f.root()), QStringLiteral("0,0"));
  QVERIFY(f.problems().isEmpty());
  f.project->undoStack()->undo();
  QVERIFY(!f.root().has("prep"));
}

void MapPlacementTest::unitsMoveButNeverOntoAnotherUnit() {
  Fixture f("srpg-prep", "orchard");
  MapPlacement pl(f.project.get(), f.doc);
  QString err;
  QCOMPARE(pl.unitAt(2, 5), 0);
  QVERIFY(!pl.moveUnit(0, 3, 4, 8, 6, &err));  // Ilsa stands there
  QVERIFY(!err.isEmpty());
  QVERIFY(pl.moveUnit(0, 4, 4, 8, 6, &err));
  QCOMPARE(f.root()["units"][0]["x"].asInt(), 4);
  QCOMPARE(f.root()["units"][0]["y"].asInt(), 4);
  QCOMPARE(f.root()["units"][0]["name"].asString(), std::string("Guard"));  // other keys untouched
  QCOMPARE(f.steps(), 1);
  QVERIFY(f.problems().isEmpty());
}

void MapPlacementTest::theCanvasToolsDoTheSame() {
  Fixture f("srpg-prep", "orchard");
  MapPage page(f.project.get(), f.doc);
  page.resize(900, 600);
  page.show();
  QTest::qWait(30);
  MapView* v = page.view();
  QWidget* vp = v->viewport();
  auto at = [&](int x, int y) { return v->viewportPointOfCell(x, y); };
  page.setTool(MapTool::Slots);
  // Click an empty square: a new slot (number 5).
  QTest::mouseClick(vp, Qt::LeftButton, Qt::NoModifier, at(4, 2));
  QCOMPARE(slotList(f.root()), QStringLiteral("0,1 0,2 0,3 1,0 4,2"));
  // Drag it elsewhere: moved, one step; the file shows nothing until the button is released.
  const int before = f.steps();
  QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, at(4, 2));
  QTest::mouseMove(vp, at(5, 2));
  QTest::mouseMove(vp, at(6, 3));
  QCOMPARE(slotList(f.root()), QStringLiteral("0,1 0,2 0,3 1,0 4,2"));
  QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, at(6, 3));
  QCOMPARE(slotList(f.root()), QStringLiteral("0,1 0,2 0,3 1,0 6,3"));
  QCOMPARE(f.steps(), before + 1);
  // A drag released on the blank border changes nothing.
  QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, at(6, 3));
  QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, QPoint(1, 1));
  QCOMPARE(f.steps(), before + 1);
  // A drag onto another slot is refused with a message.
  QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, at(6, 3));
  QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, at(0, 1));
  QCOMPARE(f.steps(), before + 1);
  QVERIFY(page.findChild<QLabel*>("mapMessage")->isVisible() || !page.findChild<QLabel*>("mapMessage")->text().isEmpty());
  // Right-click removes.
  QTest::mouseClick(vp, Qt::RightButton, Qt::NoModifier, at(6, 3));
  QCOMPARE(slotList(f.root()), QStringLiteral("0,1 0,2 0,3 1,0"));
  QVERIFY(f.problems().isEmpty());
  // Move unit tool: drag the guard.
  page.setTool(MapTool::MoveUnit);
  QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, at(2, 5));
  QTest::mouseMove(vp, at(3, 5));
  QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, at(3, 5));
  QCOMPARE(f.root()["units"][0]["x"].asInt(), 3);
  QCOMPARE(f.root()["units"][0]["y"].asInt(), 5);
  // Painting tools are unaffected: no terrain change from placing.
  QCOMPARE(f.root()["tiles"][5].asString(), std::string("........"));
}

QTEST_MAIN(MapPlacementTest)
#include "test_map_placement.moc"
