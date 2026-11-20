// "Smooth with 5000 rows": builds a generated 5000-row file and times the things a user does. The bounds are generous
// (a loaded CI machine must not fail them); the printed numbers are what to compare over time.
#include <QElapsedTimer>
#include <QScrollBar>
#include <QTableView>
#include <QtTest>

#include "json_table_model.h"
#include "project.h"
#include "table_editor.h"
#include "table_proxy.h"
#include "test_support.h"

using namespace ote_native;

class TablePerfTest : public QObject {
  Q_OBJECT
 private slots:
  void fiveThousandRowsStaySmooth();
};

void TablePerfTest::fiveThousandRowsStaySmooth() {
  ote_test::Scratch tmp;
  ote::Json arr = ote::Json::makeArray();
  for (int i = 0; i < 5000; ++i) {
    ote::Json row = ote::Json::makeObject();
    row.set("id", ote::Json::makeString("unit_" + std::to_string(i)));
    row.set("name", ote::Json::makeString("Unit number " + std::to_string(i)));
    row.set("level", ote::Json::makeNumber(i % 40));
    row.set("hp", ote::Json::makeNumber(10 + (i * 7) % 90));
    row.set("flying", ote::Json::makeBool(i % 5 == 0));
    ote::Json stats = ote::Json::makeObject();
    for (const char* k : {"str", "skl", "spd", "def", "res"}) stats.set(k, ote::Json::makeNumber((i * 3 + k[0]) % 30));
    row.set("stats", stats);
    ote::Json tags = ote::Json::makeArray();
    tags.push(ote::Json::makeString("t" + std::to_string(i % 7)));
    row.set("tags", tags);
    arr.push(row);
  }
  ote::Json root = ote::Json::makeObject();
  root.set("units", arr);
  ote_test::writeFile(tmp.file("data/units.json"), QByteArray::fromStdString(root.dump(2)));

  QElapsedTimer t;
  auto lap = [&](const char* what, qint64 limitMs) {
    const qint64 ms = t.elapsed();
    qInfo("5000 rows: %-28s %4lld ms (limit %lld)", what, (long long)ms, (long long)limitMs);
    QVERIFY2(ms < limitMs, what);
    t.restart();
  };

  t.start();
  Project project(tmp.path());
  JsonDocument* doc = project.document("data/units.json");
  QVERIFY(doc);
  lap("load + parse", 3000);
  TableEditor editor(&project, doc, {PathStep::member("units")});
  editor.resize(1280, 720);
  editor.show();
  QVERIFY(QTest::qWaitForWindowExposed(&editor));
  (void)editor.grab();
  lap("build table + first paint", 3000);

  editor.proxy()->sort(editor.model()->columnCount() > 3 ? 3 : 0, Qt::DescendingOrder);
  (void)editor.grab();
  lap("sort by hp", 2000);

  editor.setFilterText("unit_49");
  QVERIFY(editor.proxy()->rowCount() > 0 && editor.proxy()->rowCount() < 5000);
  (void)editor.grab();
  lap("filter", 2000);
  editor.setFilterText("");
  editor.proxy()->sort(-1);
  lap("clear filter + sort", 2000);

  QScrollBar* sb = editor.view()->verticalScrollBar();
  for (int i = 0; i < 20; ++i) {
    sb->setValue(sb->maximum() * i / 20);
    (void)editor.grab();
  }
  lap("20 scroll steps + paint", 3000);

  for (int r = 0; r < 50; ++r) editor.model()->setData(editor.model()->index(r * 90, 3), QString::number(r), Qt::EditRole);
  lap("50 cell edits", 2000);

  QString block;
  for (int r = 0; r < 200; ++r) block += "x" + QString::number(r) + "\tName " + QString::number(r) + "\n";
  editor.view()->setCurrentIndex(editor.proxy()->index(1000, 0));
  QVERIFY(editor.pasteTsv(block));
  lap("paste 400 cells", 3000);

  editor.model()->duplicateRows({10, 11, 12});
  editor.model()->deleteRows({20, 21, 22, 23});
  lap("duplicate + delete rows", 2000);

  while (project.undoStack()->canUndo()) project.undoStack()->undo();
  lap("undo everything", 5000);
  QVERIFY(!doc->isDirty());
  QCOMPARE(editor.model()->rowCount(), 5000);

  QCOMPARE(project.save("data/units.json").status, SaveResult::Status::Unchanged);
  editor.model()->setData(editor.model()->index(0, 3), "1", Qt::EditRole);
  QCOMPARE(project.save("data/units.json").status, SaveResult::Status::Saved);
  lap("save", 3000);
}

QTEST_MAIN(TablePerfTest)
#include "test_table_perf.moc"
