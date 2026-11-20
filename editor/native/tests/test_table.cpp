// The table model and editor: columns, typed in-place edits, row operations with undo, view-only sort/filter,
// TSV copy and paste, nested JSON editing, and the demo game's own data files.
#include <QAbstractItemModelTester>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableView>
#include <QtTest>
#include <memory>

#include "json_commands.h"
#include "json_table_model.h"
#include "json_text_dialog.h"
#include "project.h"
#include "table_editor.h"
#include "table_proxy.h"
#include "table_shape.h"
#include "test_support.h"
#include "tsv.h"

using namespace ote_native;
using ote::Json;

namespace {
const char* kRows = R"({"units": [
  {"id": "lord",   "hp": 20, "alive": true,  "stats": {"str": 6, "spd": 7}, "tags": ["a"]},
  {"id": "mage",   "hp": 9,  "alive": false, "note": "x"},
  {"id": "archer", "hp": 14, "alive": true,  "stats": {"str": 4, "spd": 9}}
]})";

// Project + table editor over a file with the rows above.
struct Fixture {
  ote_test::Scratch tmp;
  std::unique_ptr<Project> project;
  JsonDocument* doc = nullptr;
  std::unique_ptr<TableEditor> editor;
  explicit Fixture(const char* json = kRows, const QString& rel = "data/units.json") {
    ote_test::writeFile(tmp.file(rel), json);
    project = std::make_unique<Project>(tmp.path());
    doc = project->document(rel);
    auto path = findTableArray(doc->root());
    editor = std::make_unique<TableEditor>(project.get(), doc, path ? *path : JsonPath{}, nullptr);
  }
  JsonTableModel* model() { return editor->model(); }
  int col(const char* key) {
    for (int c = 0; c < model()->columnCount(); ++c)
      if (model()->columnKey(c) == key) return c;
    return -1;
  }
  QString cell(int r, const char* key) { return model()->index(r, col(key)).data().toString(); }
  QUndoStack* stack() { return project->undoStack(); }
};
}  // namespace

class TableTest : public QObject {
  Q_OBJECT
 private slots:
  void columnsAreTheUnionOfKeysInFirstSeenOrder();
  void modelPassesQtsConsistencyChecksThroughEditsAndUndo();
  void scalarEditsKeepTheirType();
  void editingAnAbsentCellAddsTheKeyAndUndoRemovesIt();
  void rowOperationsAndUndo();
  void sortAndFilterNeverTouchTheFile();
  void copyProducesTsvAndPasteAppliesItAllOrNothing();
  void tsvQuotingRoundTrips();
  void nestedCellsEditThroughJsonAndRefuseBadJson();
  void deleteKeyClearsCells();
  void demoGameDataFilesGetATable();
};

void TableTest::columnsAreTheUnionOfKeysInFirstSeenOrder() {
  Fixture f;
  QStringList cols;
  for (int c = 0; c < f.model()->columnCount(); ++c) cols << f.model()->columnKey(c);
  QCOMPARE(cols, (QStringList{"id", "hp", "alive", "stats", "tags", "note"}));
  QCOMPARE(f.model()->rowCount(), 3);
  QCOMPARE(f.cell(0, "hp"), QString("20"));
  QCOMPARE(f.cell(1, "stats"), QString());                       // absent
  QCOMPARE(f.cell(0, "stats"), QString("{\"str\":6,\"spd\":7}"));  // nested = compact JSON
}

void TableTest::modelPassesQtsConsistencyChecksThroughEditsAndUndo() {
  Fixture f;
  QAbstractItemModelTester tester(f.model(), QAbstractItemModelTester::FailureReportingMode::QtTest);
  QAbstractItemModelTester ptester(f.editor->proxy(), QAbstractItemModelTester::FailureReportingMode::QtTest);
  f.editor->proxy()->sort(1);
  f.model()->setData(f.model()->index(0, f.col("hp")), "21", Qt::EditRole);
  f.model()->addRow(1);
  f.model()->duplicateRows({0});
  f.model()->moveRows({0}, +1);
  f.model()->deleteRows({1, 2});
  while (f.stack()->canUndo()) f.stack()->undo();
  while (f.stack()->canRedo()) f.stack()->redo();
  f.project->reload("data/units.json");
  QCOMPARE(f.model()->rowCount(), 3);
}

void TableTest::scalarEditsKeepTheirType() {
  Fixture f;
  QSignalSpy rejected(f.model(), &JsonTableModel::editRejected);
  const QModelIndex hp = f.model()->index(0, f.col("hp"));
  QVERIFY(f.model()->setData(hp, "33", Qt::EditRole));
  QVERIFY(f.doc->root()["units"][0]["hp"].isNumber());
  QCOMPARE(f.doc->root()["units"][0]["hp"].asInt(), 33);

  QVERIFY(!f.model()->setData(hp, "lots", Qt::EditRole));  // text in a number cell
  QCOMPARE(rejected.count(), 1);
  QVERIFY(rejected.at(0).at(0).toString().contains("not a number"));
  QCOMPARE(f.doc->root()["units"][0]["hp"].asInt(), 33);    // unchanged
  QVERIFY(f.editor->messageLabel()->text().contains("not a number"));  // visible to the user

  QVERIFY(f.model()->setData(f.model()->index(0, f.col("id")), "42", Qt::EditRole));  // a string stays a string
  QVERIFY(f.doc->root()["units"][0]["id"].isString());
  const QModelIndex alive = f.model()->index(0, f.col("alive"));
  QCOMPARE(alive.data(Qt::CheckStateRole).toInt(), (int)Qt::Checked);
  QVERIFY(f.model()->setData(alive, Qt::Unchecked, Qt::CheckStateRole));
  QVERIFY(f.doc->root()["units"][0]["alive"].isBool());
  QVERIFY(!f.doc->root()["units"][0]["alive"].asBool());
  QVERIFY(!f.model()->setData(alive, "perhaps", Qt::EditRole));
  // Nested cells are not edited inline.
  QVERIFY(!(f.model()->flags(f.model()->index(0, f.col("stats"))) & Qt::ItemIsEditable));
  // The no-op edit does not create an undo step.
  const int n = f.stack()->count();
  f.model()->setData(hp, "33", Qt::EditRole);
  QCOMPARE(f.stack()->count(), n);
}

void TableTest::editingAnAbsentCellAddsTheKeyAndUndoRemovesIt() {
  Fixture f;
  const QModelIndex note = f.model()->index(0, f.col("note"));
  QCOMPARE(note.data().toString(), QString());
  QVERIFY(f.model()->setData(note, "hello", Qt::EditRole));
  QCOMPARE(QString::fromStdString(f.doc->root()["units"][0]["note"].asString()), QString("hello"));
  f.stack()->undo();
  QVERIFY(!f.doc->root()["units"][0].has("note"));
  // An absent cell with no usual type takes JSON if the text is JSON.
  QVERIFY(f.model()->setData(f.model()->index(2, f.col("tags")), "[1, 2]", Qt::EditRole));  // absent: JSON if it is JSON
  QVERIFY(f.doc->root()["units"][2]["tags"].isArray());
}

void TableTest::rowOperationsAndUndo() {
  Fixture f;
  const QByteArray original = f.doc->serialize();
  f.model()->addRow(1);
  QCOMPARE(f.model()->rowCount(), 4);
  // Defaults follow the columns' usual types: "" 0 false {} [] .
  const Json& added = f.doc->root()["units"][1];
  QVERIFY(added["id"].isString() && added["hp"].isNumber() && added["alive"].isBool());
  QVERIFY(added["stats"].isObject() && added["tags"].isArray());

  f.model()->duplicateRows({0});
  QCOMPARE(f.model()->rowCount(), 5);
  QCOMPARE(QString::fromStdString(f.doc->root()["units"][1]["id"].asString()), QString("lord_copy"));  // new id, no clash
  f.model()->duplicateRows({0});
  QCOMPARE(QString::fromStdString(f.doc->root()["units"][1]["id"].asString()), QString("lord_copy2"));

  f.model()->moveRows({0}, +1);
  QCOMPARE(QString::fromStdString(f.doc->root()["units"][1]["id"].asString()), QString("lord"));
  f.model()->moveRows({0}, -1);  // already first: refused, no new step
  const int steps = f.stack()->count();
  f.model()->moveRows({0}, -1);
  QCOMPARE(f.stack()->count(), steps);

  f.model()->deleteRows({1, 2});
  QCOMPARE(f.model()->rowCount(), 4);
  while (f.stack()->canUndo()) f.stack()->undo();
  QCOMPARE(f.doc->serialize(), original);  // byte-identical after undoing every row operation
  QVERIFY(!f.doc->isDirty());
}

void TableTest::sortAndFilterNeverTouchTheFile() {
  Fixture f;
  TableProxy* p = f.editor->proxy();
  p->sort(f.col("hp"), Qt::AscendingOrder);
  QCOMPARE(p->index(0, f.col("id")).data().toString(), QString("mage"));  // 9 < 14 < 20: numeric, not "14" < "20" < "9"
  QCOMPARE(p->index(2, f.col("id")).data().toString(), QString("lord"));
  p->sort(f.col("hp"), Qt::DescendingOrder);
  QCOMPARE(p->index(0, f.col("id")).data().toString(), QString("lord"));
  f.editor->setFilterText("arch");
  QCOMPARE(p->rowCount(), 1);
  QCOMPARE(p->index(0, 0).data().toString(), QString("archer"));
  QVERIFY(p->isViewOnlyOrder());
  QVERIFY(!f.editor->findChild<QPushButton*>("moveUpButton")->isEnabled());  // moving rows while sorted would be a lie
  // Editing through the proxy hits the right row of the file.
  QVERIFY(p->setData(p->index(0, f.col("hp")), "15", Qt::EditRole));
  QCOMPARE(f.doc->root()["units"][2]["hp"].asInt(), 15);
  // A sorted view cannot reorder the file.
  f.editor->moveSelectedRows(+1);
  f.editor->setFilterText("");
  p->sort(-1);
  QCOMPARE(f.doc->root()["units"][0]["id"].asString(), std::string("lord"));
  QCOMPARE(f.doc->root()["units"][1]["id"].asString(), std::string("mage"));
  QCOMPARE(f.stack()->count(), 1);  // only the one real edit
}

void TableTest::copyProducesTsvAndPasteAppliesItAllOrNothing() {
  Fixture f;
  QTableView* v = f.editor->view();
  // Select id+hp of the first two rows.
  v->selectionModel()->select(QItemSelection(v->model()->index(0, f.col("id")), v->model()->index(1, f.col("hp"))),
                              QItemSelectionModel::ClearAndSelect);
  QCOMPARE(f.editor->selectionAsTsv(), QString("lord\t20\nmage\t9"));

  // Paste a 2x2 block (as a spreadsheet would send it) at row 2, "id".
  v->setCurrentIndex(v->model()->index(2, f.col("id")));
  QVERIFY(f.editor->pasteTsv("a1\t5\nb2\t6\n"));  // second row falls outside the table: skipped, not an error
  QCOMPARE(f.cell(2, "id"), QString("a1"));
  QCOMPARE(f.cell(2, "hp"), QString("5"));
  QVERIFY(f.doc->root()["units"][2]["hp"].isNumber());
  QVERIFY(f.editor->messageLabel()->text().contains("left out"));
  QCOMPARE(f.stack()->count(), 1);  // one undo step for the whole paste
  f.stack()->undo();
  QCOMPARE(f.cell(2, "id"), QString("archer"));

  // One bad cell: nothing changes.
  const QByteArray before = f.doc->serialize();
  v->setCurrentIndex(v->model()->index(0, f.col("id")));
  QVERIFY(!f.editor->pasteTsv("zzz\t77\nyyy\tnot-a-number"));
  QCOMPARE(f.doc->serialize(), before);
  QVERIFY(f.editor->messageLabel()->text().contains("Nothing was pasted"));
  QVERIFY(f.editor->messageLabel()->text().contains("not a number"));

  // Blank cells from a spreadsheet leave numbers alone.
  QVERIFY(f.editor->pasteTsv("q\t\n"));
  QCOMPARE(f.cell(0, "id"), QString("q"));
  QCOMPARE(f.cell(0, "hp"), QString("20"));
}

void TableTest::tsvQuotingRoundTrips() {
  const QList<QStringList> rows{{"plain", "has\ttab", "has \"quote\""}, {"two\nlines", "", "x"}};
  const QString t = toTsv(rows);
  QCOMPARE(parseTsv(t), rows);
  QCOMPARE(parseTsv("a\tb\r\nc\td\r\n"), (QList<QStringList>{{"a", "b"}, {"c", "d"}}));
  QCOMPARE(parseTsv("").size(), 0);
  QCOMPARE(parseTsv("single").size(), 1);
  QCOMPARE(parseTsv("\"{\"\"a\"\":1}\"\tx"), (QList<QStringList>{{"{\"a\":1}", "x"}}));
}

void TableTest::nestedCellsEditThroughJsonAndRefuseBadJson() {
  Fixture f;
  // The dialog refuses invalid JSON.
  JsonTextDialog dlg("t", f.doc->root()["units"][0]["stats"]);
  QVERIFY(dlg.isValid());
  dlg.setText("{\"str\": 6,");
  QVERIFY(!dlg.isValid());
  QVERIFY(!dlg.okButton()->isEnabled());
  QVERIFY(dlg.messageLabel()->text().contains("Not valid JSON"));
  dlg.accept();
  QVERIFY(dlg.result() != QDialog::Accepted);
  dlg.setText("{\"str\": 7, \"spd\": 7}");
  QVERIFY(dlg.isValid());
  QVERIFY(dlg.okButton()->isEnabled());

  // The editor applies an accepted value to the right cell, as one undo step.
  QString seenTitle;
  f.editor->setNestedEditorHook([&](const QString& title, const Json& initial, Json* out) {
    seenTitle = title;
    *out = initial;
    out->set("str", Json::makeNumber(8));
    return true;
  });
  f.editor->editNestedCell(f.editor->proxy()->index(0, f.col("stats")));
  QVERIFY(seenTitle.contains("stats"));
  QCOMPARE(f.doc->root()["units"][0]["stats"]["str"].asInt(), 8);
  QCOMPARE(f.doc->root()["units"][0]["stats"]["spd"].asInt(), 7);  // the rest of the object is intact
  f.stack()->undo();
  QCOMPARE(f.doc->root()["units"][0]["stats"]["str"].asInt(), 6);
  // A scalar cell is not opened as nested.
  seenTitle.clear();
  f.editor->editNestedCell(f.editor->proxy()->index(0, f.col("hp")));
  QVERIFY(seenTitle.isEmpty());
  // Cancel leaves the data alone.
  f.editor->setNestedEditorHook([](const QString&, const Json&, Json*) { return false; });
  f.editor->editNestedCell(f.editor->proxy()->index(0, f.col("stats")));
  QCOMPARE(f.doc->root()["units"][0]["stats"]["str"].asInt(), 6);
}

void TableTest::deleteKeyClearsCells() {
  Fixture f;
  QTableView* v = f.editor->view();
  v->selectionModel()->select(v->model()->index(1, f.col("note")), QItemSelectionModel::ClearAndSelect);
  f.editor->clearSelectedCells();
  QVERIFY(!f.doc->root()["units"][1].has("note"));
  f.stack()->undo();
  QVERIFY(f.doc->root()["units"][1].has("note"));
}

void TableTest::demoGameDataFilesGetATable() {
  ote_test::Scratch tmp;
  const QString game = ote_test::copyDemoGame(tmp);
  Project project(game);
  for (const char* rel : {"data/units.json", "data/weapons.json", "data/terrain.json", "data/items.json", "data/classes.json"}) {
    if (!QFile::exists(game + "/" + rel)) continue;  // the demo has no classes.json: skip rather than fail
    JsonDocument* doc = project.document(rel);
    QVERIFY2(doc, rel);
    auto path = findTableArray(doc->root());
    QVERIFY2(path.has_value(), rel);
    TableEditor ed(&project, doc, *path);
    QAbstractItemModelTester tester(ed.model(), QAbstractItemModelTester::FailureReportingMode::QtTest);
    QVERIFY2(ed.model()->rowCount() > 0, rel);
    QVERIFY2(ed.model()->columnCount() > 1, rel);
    // Every cell reads without crashing, and "id" is the first column in these files.
    for (int r = 0; r < ed.model()->rowCount(); ++r)
      for (int c = 0; c < ed.model()->columnCount(); ++c) (void)ed.model()->index(r, c).data();
  }
  TableEditor units(&project, project.document("data/units.json"), *findTableArray(project.document("data/units.json")->root()));
  QCOMPARE(units.model()->columnKey(0), QString("id"));
}

QTEST_MAIN(TableTest)
#include "test_table.moc"
