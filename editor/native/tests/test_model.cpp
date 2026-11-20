// Model tests: edit operations with undo/redo, dirty tracking, save and conflicts, comments, file watching,
// cell text rules, table shape detection.
#include <QtTest>

#include "cell_text.h"
#include "comment_scan.h"
#include "json_commands.h"
#include "project.h"
#include "table_shape.h"
#include "test_support.h"

using namespace ote_native;
using ote::Json;

namespace {
// A file in the exact form the editor writes, so "unchanged" can be compared byte for byte.
QByteArray canonical(const char* text) {
  Json j;
  std::string err;
  Json::parse(text, j, err);
  return QByteArray::fromStdString(j.dump(2) + "\n");
}
const char* kUnits =
    R"({"zeta": 1, "units": [{"id": "a", "hp": 5, "extra": {"k": [1, 2]}}, {"id": "b", "hp": 7}], "alpha": "last"})";
}  // namespace

class ModelTest : public QObject {
  Q_OBJECT
 private slots:
  void everyOperationRoundTripsByteIdentical();
  void unknownKeysAndOrderSurviveAnEditAndSave();
  void dirtyTrackingFollowsUndoAndSave();
  void savedStateUndoneThenNewEditStaysDirty();
  void conflictIsReportedAndNothingIsOverwritten();
  void commentsAreDetected();
  void externalChangeIsFlagged();
  void reloadTurnsOldCommandsIntoNoOps();
  void cellTextKeepsTypes();
  void tableShapeDetection();
  void invalidJsonFileIsRefused();
};

void ModelTest::everyOperationRoundTripsByteIdentical() {
  ote_test::Scratch tmp;
  ote_test::writeFile(tmp.file("data/units.json"), canonical(kUnits));
  Project project(tmp.path());
  JsonDocument* doc = project.document("data/units.json");
  QVERIFY(doc);
  const QByteArray original = doc->serialize();
  QCOMPARE(original, ote_test::readFile(tmp.file("data/units.json")));

  const JsonPath units{PathStep::member("units")};
  auto row = [&](size_t i) { JsonPath p = units; p.push_back(PathStep::at(i)); return p; };
  auto field = [&](size_t i, const char* k) { JsonPath p = row(i); p.push_back(PathStep::member(k)); return p; };

  QList<QByteArray> states{original};
  auto push = [&](QUndoCommand* c) { project.push(doc, c); states << doc->serialize(); };

  push(new SetValueCommand(doc, field(0, "hp"), Json::makeNumber(99)));
  push(new InsertCommand(doc, row(1), 1, "mov", Json::makeNumber(4)));            // new key in the middle
  push(new InsertCommand(doc, units, 2, "", Json::makeObject()));                  // new row
  push(new MoveCommand(doc, units, 0, 2));                                         // reorder rows
  push(new MoveCommand(doc, row(0), 0, 1));                                        // reorder keys of an object
  push(new EraseCommand(doc, row(1)));                                             // delete a row
  push(new EraseCommand(doc, JsonPath{PathStep::member("zeta")}));                 // delete a top-level key
  push(new InsertCommand(doc, JsonPath{}, 0, "first", Json::makeBool(true)));      // insert at the front of the root
  QVERIFY2(states.last() != original, "edits changed the document");

  // Undo everything, checking each step against the recorded state.
  for (int i = states.size() - 2; i >= 0; --i) {
    project.undoStack()->undo();
    QCOMPARE(doc->serialize(), states[i]);
  }
  QCOMPARE(doc->serialize(), original);
  QVERIFY(!doc->isDirty());
  // And redo everything.
  for (int i = 1; i < states.size(); ++i) {
    project.undoStack()->redo();
    QCOMPARE(doc->serialize(), states[i]);
  }
}

void ModelTest::unknownKeysAndOrderSurviveAnEditAndSave() {
  ote_test::Scratch tmp;
  ote_test::writeFile(tmp.file("data/units.json"), canonical(kUnits));
  Project project(tmp.path());
  JsonDocument* doc = project.document("data/units.json");
  project.push(doc, new SetValueCommand(doc, {PathStep::member("units"), PathStep::at(1), PathStep::member("hp")}, Json::makeNumber(8)));
  QCOMPARE(project.save("data/units.json").status, SaveResult::Status::Saved);

  Json back;
  std::string err;
  QVERIFY(Json::parse(ote_test::readFile(tmp.file("data/units.json")).toStdString(), back, err));
  QCOMPARE(back.members().size(), (size_t)3);
  QCOMPARE(QString::fromStdString(back.members()[0].first), QString("zeta"));
  QCOMPARE(QString::fromStdString(back.members()[1].first), QString("units"));
  QCOMPARE(QString::fromStdString(back.members()[2].first), QString("alpha"));
  QCOMPARE(back["units"][0].members().size(), (size_t)3);  // "extra" is kept
  QCOMPARE(back["units"][0].members()[2].second["k"][1].asInt(), 2);
  QCOMPARE(back["units"][1]["hp"].asInt(), 8);
}

void ModelTest::dirtyTrackingFollowsUndoAndSave() {
  ote_test::Scratch tmp;
  ote_test::writeFile(tmp.file("a.json"), canonical(kUnits));
  ote_test::writeFile(tmp.file("b.json"), canonical(kUnits));
  Project project(tmp.path());
  JsonDocument* a = project.document("a.json");
  JsonDocument* b = project.document("b.json");
  const JsonPath hp{PathStep::member("zeta")};
  QVERIFY(!a->isDirty());
  project.push(a, new SetValueCommand(a, hp, Json::makeNumber(2)));
  QVERIFY(a->isDirty());
  QVERIFY(!b->isDirty());  // dirty is per file
  QCOMPARE(project.dirtyFiles(), QStringList{"a.json"});
  project.undoStack()->undo();
  QVERIFY(!a->isDirty());  // back at the loaded state
  project.undoStack()->redo();
  QCOMPARE(project.save("a.json").status, SaveResult::Status::Saved);
  QVERIFY(!a->isDirty());
  project.undoStack()->undo();
  QVERIFY(a->isDirty());   // before the saved state
  project.undoStack()->redo();
  QVERIFY(!a->isDirty());
}

void ModelTest::savedStateUndoneThenNewEditStaysDirty() {
  ote_test::Scratch tmp;
  ote_test::writeFile(tmp.file("a.json"), canonical(kUnits));
  Project project(tmp.path());
  JsonDocument* a = project.document("a.json");
  const JsonPath p{PathStep::member("zeta")};
  project.push(a, new SetValueCommand(a, p, Json::makeNumber(2)));
  project.save("a.json");
  project.undoStack()->undo();
  project.push(a, new SetValueCommand(a, p, Json::makeNumber(3)));  // same version number as the saved one, other content
  QVERIFY(a->isDirty());
}

void ModelTest::conflictIsReportedAndNothingIsOverwritten() {
  ote_test::Scratch tmp;
  ote_test::writeFile(tmp.file("a.json"), canonical(kUnits));
  Project project(tmp.path());
  JsonDocument* a = project.document("a.json");
  project.push(a, new SetValueCommand(a, {PathStep::member("zeta")}, Json::makeNumber(2)));
  const QByteArray theirs = "{\"zeta\": 100}\n";
  ote_test::writeFile(tmp.file("a.json"), theirs);

  SaveResult r = project.save("a.json");
  QCOMPARE(r.status, SaveResult::Status::Conflict);
  QCOMPARE(ote_test::readFile(tmp.file("a.json")), theirs);  // untouched
  QVERIFY(a->isDirty());

  r = project.save("a.json", /*overwrite=*/true);  // "keep mine"
  QCOMPARE(r.status, SaveResult::Status::Saved);
  QCOMPARE(ote_test::readFile(tmp.file("a.json")), a->serialize());
  QVERIFY(!a->isDirty());

  // A deleted file is a conflict too.
  project.push(a, new SetValueCommand(a, {PathStep::member("zeta")}, Json::makeNumber(5)));
  QFile::remove(tmp.file("a.json"));
  QCOMPARE(project.save("a.json").status, SaveResult::Status::Conflict);
  QVERIFY(!QFile::exists(tmp.file("a.json")));
}

void ModelTest::commentsAreDetected() {
  QVERIFY(jsonTextHasComments("// top\n{\"a\": 1}"));
  QVERIFY(jsonTextHasComments("{\"a\": 1 /* x */}"));
  QVERIFY(!jsonTextHasComments("{\"url\": \"http://x.y\", \"q\": \"a\\\"//b\"}"));
  ote_test::Scratch tmp;
  ote_test::writeFile(tmp.file("c.json"), "// note\n{\"a\": 1}\n");
  ote_test::writeFile(tmp.file("p.json"), "{\"a\": 1}\n");
  Project project(tmp.path());
  QVERIFY(project.document("c.json")->hasComments());
  QVERIFY(!project.document("p.json")->hasComments());
}

void ModelTest::externalChangeIsFlagged() {
  ote_test::Scratch tmp;
  ote_test::writeFile(tmp.file("a.json"), canonical(kUnits));
  Project project(tmp.path());
  JsonDocument* a = project.document("a.json");
  QSignalSpy spy(&project, &Project::fileStateChanged);
  QVERIFY(!a->externallyChanged());
  QTest::qWait(50);
  ote_test::writeFile(tmp.file("a.json"), "{\"zeta\": 9}\n");
  QTRY_VERIFY_WITH_TIMEOUT(a->externallyChanged(), 5000);  // found by the QFileSystemWatcher, no explicit check
  QVERIFY(spy.count() >= 1);
  // Our own save is not an external change.
  project.reload("a.json");
  QVERIFY(!a->externallyChanged());
  project.push(a, new SetValueCommand(a, {PathStep::member("zeta")}, Json::makeNumber(3)));
  QCOMPARE(project.save("a.json").status, SaveResult::Status::Saved);
  QTest::qWait(400);
  QVERIFY(!a->externallyChanged());
  // A new file appears in the file list.
  QSignalSpy lists(&project, &Project::fileListChanged);
  ote_test::writeFile(tmp.file("new.json"), "{}\n");
  QTRY_VERIFY_WITH_TIMEOUT(lists.count() >= 1, 5000);
  QVERIFY(project.jsonFiles().contains("new.json"));
}

void ModelTest::reloadTurnsOldCommandsIntoNoOps() {
  ote_test::Scratch tmp;
  ote_test::writeFile(tmp.file("a.json"), canonical(kUnits));
  Project project(tmp.path());
  JsonDocument* a = project.document("a.json");
  project.push(a, new SetValueCommand(a, {PathStep::member("zeta")}, Json::makeNumber(2)));
  ote_test::writeFile(tmp.file("a.json"), "{\"zeta\": 50}\n");
  QVERIFY(project.reload("a.json"));
  QVERIFY(!a->isDirty());
  project.undoStack()->undo();  // the command is from the old tree: must not touch the new one
  project.undoStack()->redo();
  QCOMPARE(a->root()["zeta"].asInt(), 50);
  QVERIFY(!a->isDirty());
}

void ModelTest::cellTextKeepsTypes() {
  Json n = Json::makeNumber(5), s = Json::makeString("x"), b = Json::makeBool(true), o = Json::makeObject();
  CellParse r = parseCellText("12.5", &n, Json::Type::Null);
  QVERIFY(r.ok && r.value.isNumber() && r.value.asNumber() == 12.5);
  r = parseCellText("abc", &n, Json::Type::Null);
  QVERIFY(!r.ok);
  QVERIFY(r.error.contains("not a number"));
  QVERIFY(!parseCellText("", &n, Json::Type::Null).ok);
  QVERIFY(!parseCellText("nan", &n, Json::Type::Null).ok);
  r = parseCellText("42", &s, Json::Type::Null);
  QVERIFY(r.ok && r.value.isString() && r.value.asString() == "42");  // a string stays a string
  r = parseCellText("FALSE", &b, Json::Type::Null);
  QVERIFY(r.ok && r.value.isBool() && !r.value.asBool());
  QVERIFY(!parseCellText("maybe", &b, Json::Type::Null).ok);
  r = parseCellText("{\"a\": [1]}", &o, Json::Type::Null);
  QVERIFY(r.ok && r.value.isObject());
  QVERIFY(!parseCellText("{broken", &o, Json::Type::Null).ok);
  r = parseCellText("7", nullptr, Json::Type::Number);  // absent cell: the column decides
  QVERIFY(r.ok && r.value.isNumber());
  QCOMPARE(jsonToCellText(Json::makeNumber(0.1)), QString("0.1"));
  QCOMPARE(jsonToCellText(Json::makeNumber(12)), QString("12"));
  QCOMPARE(jsonToCellText(o), QString("{}"));
}

void ModelTest::tableShapeDetection() {
  Json j;
  std::string err;
  Json::parse(R"([{"a":1},{"b":2}])", j, err);
  QVERIFY(findTableArray(j) && findTableArray(j)->empty());
  Json::parse(R"({"units":[{"a":1}]})", j, err);
  QVERIFY(findTableArray(j) && findTableArray(j)->size() == 1);
  Json::parse(R"({"units":[{"a":1}], "other": 2})", j, err);
  QVERIFY(!findTableArray(j));
  Json::parse(R"({"units":[1,2]})", j, err);
  QVERIFY(!findTableArray(j));
  Json::parse(R"({"units":[]})", j, err);
  QVERIFY(!findTableArray(j));
}

void ModelTest::invalidJsonFileIsRefused() {
  ote_test::Scratch tmp;
  ote_test::writeFile(tmp.file("bad.json"), "{ nope");
  Project project(tmp.path());
  QString err;
  QVERIFY(!project.document("bad.json", &err));
  QVERIFY(err.contains("bad.json"));
}

QTEST_MAIN(ModelTest)
#include "test_model.moc"
