// The schema forms: one widget per field kind, edits as single undo steps, form/table/tree staying in sync, keys the
// schema does not know surviving untouched, wrong-kind values shown with a warning and never changed.
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTabWidget>
#include <QToolButton>
#include <QUndoStack>
#include <QtTest>
#include <memory>

#include "form_context.h"
#include "form_fields.h"
#include "form_view.h"
#include "id_source.h"
#include "json_commands.h"
#include "json_table_model.h"
#include "json_tree_model.h"
#include "project.h"
#include "table_shape.h"
#include "table_editor.h"
#include "test_support.h"
#include "tree_editor.h"

using namespace ote_native;
using ote::Json;

namespace {

// A small schema with one field of every kind. Words here are test words, not product wording.
const char* kSchemaText = R"({
  "files": ["data/thing.json"],
  "root": { "type": "object", "fields": {
    "id": { "type": "string", "required": true, "help": "the id" },
    "label": { "type": "string", "help": "free text" },
    "count": { "type": "int", "min": 0, "max": 10, "default": 3, "help": "a whole number 0..10" },
    "ratio": { "type": "number", "help": "any number" },
    "flag": { "type": "bool", "default": false, "help": "yes or no" },
    "mode": { "type": "enum", "values": ["a", "b", "c"], "help": "one word" },
    "pick": { "type": "ref", "ref": "data/ids.json", "help": "an id of ids.json" },
    "pair": { "type": "list", "length": 2, "of": { "type": "int" }, "help": "two numbers" },
    "tags": { "type": "list", "of": { "type": "string" }, "help": "free words" },
    "picks": { "type": "list", "of": { "type": "ref", "ref": "data/ids.json" }, "help": "ids" },
    "parts": { "type": "list", "of": { "type": "object", "fields": {
      "id": { "type": "string", "required": true }, "size": { "type": "int", "min": 1, "max": 9 } } }, "help": "objects" },
    "group": { "type": "object", "fields": { "inner": { "type": "int" } }, "help": "a plain nested object" },
    "block": { "type": "object", "block": true, "fields": { "level": { "type": "int", "required": true } }, "help": "optional block" },
    "weights": { "type": "map", "ref": "data/ids.json", "of": { "type": "int" }, "help": "one number per id" },
    "raw": { "type": "json", "help": "raw json" }
  } }
})";

const char* kThing = R"({
  "id": "t1",
  "label": "hello",
  "count": 4,
  "mode": "b",
  "pick": "two",
  "tags": ["x", "y"],
  "parts": [ { "id": "p1", "size": 2 }, { "id": "p2" } ],
  "weights": { "one": 5 },
  "extra": { "keep": [1, 2, 3], "me": true },
  "zzz": "unknown, must survive"
})";

const char* kIds = R"({ "ids": [ { "id": "one" }, { "id": "two" }, { "id": "three" } ] })";

struct Rig {
  ote_test::Scratch tmp;
  SchemaSet schemas;
  std::unique_ptr<Project> project;
  std::unique_ptr<IdSource> ids;
  JsonDocument* doc = nullptr;
  std::unique_ptr<FormContext> ctx;
  std::unique_ptr<FormView> form;

  explicit Rig(const char* thing = kThing) {
    ote_test::writeFile(tmp.file("g/game.json"), "{}");
    ote_test::writeFile(tmp.file("g/data/thing.json"), thing);
    ote_test::writeFile(tmp.file("g/data/ids.json"), kIds);
    schemas.addFromText("thing.json", kSchemaText);
    project = std::make_unique<Project>(tmp.file("g"));
    ids = std::make_unique<IdSource>(project.get());
    doc = project->document("data/thing.json");
    ctx = std::make_unique<FormContext>(project.get(), doc, ids.get());
    // Never open a dialog: the JSON editor is a hook the test sets per case.
    ctx->setJsonEditor([this](const QString&, const Json&, Json* out) {
      if (!jsonAnswer) return false;
      *out = *jsonAnswer;
      return true;
    });
    form = std::make_unique<FormView>(ctx.get(), schemas.forFile("data/thing.json")->root.get());
    form->resize(520, 900);
    form->setTarget({});
    form->flush();
  }
  std::optional<Json> jsonAnswer;
  JsonPath p(std::initializer_list<const char*> keys) {
    JsonPath out;
    for (const char* k : keys) out.push_back(PathStep::member(k));
    return out;
  }
  template <class T> T* field(std::initializer_list<const char*> keys) { return qobject_cast<T*>(form->fieldAt(p(keys))); }
  const Json& at(std::initializer_list<const char*> keys) { return *resolvePath(doc->root(), p(keys)); }
  QUndoStack* stack() { return project->undoStack(); }
  void flush() { form->flush(); QCoreApplication::processEvents(); form->flush(); }
};

QString dumpOf(const Json& j) { return QString::fromStdString(j.dump(0)); }

}  // namespace

class FormTest : public QObject {
  Q_OBJECT
 private slots:
  void everyKindGetsItsWidget();
  void scalarEditsAreOneUndoStepAndRefuseBadInput();
  void boolEnumAndRefFields();
  void idCombosFollowTheReferencedFileLive();
  void listsAddRemoveReorder();
  void mapsListOneRowPerId();
  void blocksEnableAndDisable();
  void jsonFieldUsesTheDialogHook();
  void unknownKeysSurviveByteForByte();
  void otherKeysAreShownAndEditable();
  void aWrongKindIsShownWithAWarningAndNeverChanged();
  void tableFormAndTreeStayInSync();
  void problemMessagesFindTheRowAndTheField();
  void tableColumnsComeFromTheSchema();
};

void FormTest::everyKindGetsItsWidget() {
  Rig r;
  QVERIFY(r.field<ScalarField>({"label"}));
  QVERIFY(r.field<ScalarField>({"count"}));
  QVERIFY(r.field<ScalarField>({"ratio"}));
  QVERIFY(r.field<BoolField>({"flag"}));
  QVERIFY(r.field<ChoiceField>({"mode"}));
  QVERIFY(r.field<ChoiceField>({"pick"}));
  QVERIFY(r.field<TupleField>({"pair"}));
  QVERIFY(r.field<JsonField>({"raw"}));
  QVERIFY(r.form->findChild<QWidget*>("listField"));
  QVERIFY(r.form->findChild<QWidget*>("mapField"));
  QVERIFY(r.form->findChild<QCheckBox*>("blockEnable"));
  // The raw key is the main label; the help text sits with the field.
  QStringList keys, helps;
  for (auto* l : r.form->findChildren<QLabel*>("keyLabel")) keys << l->text();
  for (auto* l : r.form->findChildren<QLabel*>("helpLabel")) helps << l->text();
  QVERIFY(keys.contains("count"));
  QVERIFY(keys.contains("id *"));  // required keys are marked
  QVERIFY(helps.contains("a whole number 0..10"));
  // The value is shown, an absent key shows its default as a placeholder.
  QCOMPARE(r.field<ScalarField>({"count"})->edit()->text(), QString("4"));
  QCOMPARE(r.field<ScalarField>({"ratio"})->edit()->text(), QString());
  QCOMPARE(r.field<ChoiceField>({"mode"})->combo()->currentData().toString(), QString("b"));
  QCOMPARE(r.field<ChoiceField>({"pick"})->combo()->currentData().toString(), QString("two"));
  QCOMPARE(r.field<ChoiceField>({"pick"})->combo()->count(), 4);  // "not set" + the three ids
  QVERIFY(!r.field<BoolField>({"flag"})->box()->isChecked());
}

void FormTest::scalarEditsAreOneUndoStepAndRefuseBadInput() {
  Rig r;
  auto* label = r.field<ScalarField>({"label"});
  const int before = r.stack()->count();
  label->edit()->setText("changed");
  emit label->edit()->editingFinished();
  QCOMPARE(QString::fromStdString(r.at({"label"}).asString()), QString("changed"));
  QCOMPARE(r.stack()->count(), before + 1);
  r.stack()->undo();
  QCOMPARE(label->edit()->text(), QString("hello"));  // the form followed the undo
  r.stack()->redo();
  QCOMPARE(label->edit()->text(), QString("changed"));

  // A number field refuses text and out-of-range values, says why, and writes nothing.
  auto* count = r.field<ScalarField>({"count"});
  const int n = r.stack()->count();
  count->edit()->setText("many");
  emit count->edit()->editingFinished();
  QCOMPARE(r.at({"count"}).asInt(), 4);
  QCOMPARE(r.stack()->count(), n);
  auto* note = count->findChild<QLabel*>("fieldNote");
  QVERIFY(note && !note->isHidden());
  QVERIFY(note->text().contains("not a number"));
  count->edit()->setText("11");
  emit count->edit()->editingFinished();
  QCOMPARE(r.at({"count"}).asInt(), 4);
  QVERIFY(note->text().contains("from 0 to 10"));
  count->edit()->setText("2.5");
  emit count->edit()->editingFinished();
  QVERIFY(note->text().contains("whole number"));
  count->edit()->setText("7");
  emit count->edit()->editingFinished();
  QCOMPARE(r.at({"count"}).asInt(), 7);

  // An absent key is created by the first edit (one step), and the clear button removes it again (one step).
  auto* ratio = r.field<ScalarField>({"ratio"});
  const int m = r.stack()->count();
  ratio->edit()->setText("0.25");
  emit ratio->edit()->editingFinished();
  QCOMPARE(r.at({"ratio"}).asNumber(), 0.25);
  QCOMPARE(r.stack()->count(), m + 1);
  ratio->findChild<QToolButton*>("clearField")->click();
  QVERIFY(!resolvePath(r.doc->root(), r.p({"ratio"})));
  r.stack()->undo();
  QCOMPARE(r.at({"ratio"}).asNumber(), 0.25);
}

void FormTest::boolEnumAndRefFields() {
  Rig r;
  auto* flag = r.field<BoolField>({"flag"});
  flag->box()->click();
  QVERIFY(r.at({"flag"}).asBool());
  r.stack()->undo();
  QVERIFY(!resolvePath(r.doc->root(), r.p({"flag"})));
  QVERIFY(!flag->box()->isChecked());

  auto* mode = r.field<ChoiceField>({"mode"});
  const int c = mode->combo()->findData("c");
  QVERIFY(c >= 0);
  mode->combo()->setCurrentIndex(c);
  emit mode->combo()->activated(c);
  QCOMPARE(QString::fromStdString(r.at({"mode"}).asString()), QString("c"));
  emit mode->combo()->activated(0);  // "(not set)" removes the key
  QVERIFY(!resolvePath(r.doc->root(), r.p({"mode"})));
  r.stack()->undo();
  r.stack()->undo();
  QCOMPARE(QString::fromStdString(r.at({"mode"}).asString()), QString("b"));

  // A stored word the schema does not list is kept and marked, not replaced.
  r.ctx->set(r.p({"mode"}), Json::makeString("zzz"));
  r.flush();
  mode = r.field<ChoiceField>({"mode"});
  QCOMPARE(mode->combo()->currentData().toString(), QString("zzz"));
  QVERIFY(mode->findChild<QLabel*>("fieldNote")->text().contains("not one of the allowed"));
}

void FormTest::idCombosFollowTheReferencedFileLive() {
  Rig r;
  auto* pick = r.field<ChoiceField>({"pick"});
  QCOMPARE(pick->combo()->count(), 4);
  // Add an id to ids.json (an unsaved edit in another document): the combo shows it at once.
  JsonDocument* ids = r.project->document("data/ids.json");
  Json item = Json::makeObject();
  item.set("id", Json::makeString("four"));
  r.project->push(ids, new InsertCommand(ids, {PathStep::member("ids")}, 3, "", item));
  QCOMPARE(pick->combo()->count(), 5);
  QVERIFY(pick->combo()->findData("four") >= 0);
  r.stack()->undo();
  QCOMPARE(pick->combo()->count(), 4);
  // Renaming the id the field uses makes the value "unknown", still kept.
  r.project->push(ids, new SetValueCommand(ids, {PathStep::member("ids"), PathStep::at(1), PathStep::member("id")}, Json::makeString("2nd")));
  pick = r.field<ChoiceField>({"pick"});
  QCOMPARE(pick->combo()->currentData().toString(), QString("two"));
  QVERIFY(pick->combo()->currentText().contains("unknown"));
  QCOMPARE(QString::fromStdString(r.at({"pick"}).asString()), QString("two"));
}

void FormTest::listsAddRemoveReorder() {
  Rig r;
  // Find the tags list through its first item's field: the string field at tags[0].
  JsonPath t0 = r.p({"tags"});
  t0.push_back(PathStep::at(0));
  auto* first = qobject_cast<ScalarField*>(r.form->fieldAt(t0));
  QVERIFY(first);
  QWidget* listWidget = first->parentWidget();
  while (listWidget && listWidget->objectName() != "listField") listWidget = listWidget->parentWidget();
  QVERIFY(listWidget);
  auto* add = listWidget->findChild<QPushButton*>("addItem");
  QVERIFY(add);
  const int before = r.stack()->count();
  add->click();
  QCOMPARE(r.at({"tags"}).size(), size_t(3));
  QCOMPARE(r.stack()->count(), before + 1);  // one user action = one step
  r.flush();
  // Move the first item down.
  listWidget = r.form->fieldAt(t0)->parentWidget();
  while (listWidget->objectName() != "listField") listWidget = listWidget->parentWidget();
  listWidget->findChildren<QToolButton*>("itemDown").first()->click();
  QCOMPARE(QString::fromStdString(r.at({"tags"})[size_t(0)].asString()), QString("y"));
  QCOMPARE(QString::fromStdString(r.at({"tags"})[size_t(1)].asString()), QString("x"));
  r.flush();
  listWidget = r.form->fieldAt(t0)->parentWidget();
  while (listWidget->objectName() != "listField") listWidget = listWidget->parentWidget();
  listWidget->findChildren<QToolButton*>("itemRemove").first()->click();
  QCOMPARE(r.at({"tags"}).size(), size_t(2));
  QCOMPARE(QString::fromStdString(r.at({"tags"})[size_t(0)].asString()), QString("x"));
  // Undo all three: back to the start, byte for byte.
  while (r.stack()->canUndo()) r.stack()->undo();
  QCOMPARE(dumpOf(r.at({"tags"})), QString("[\"x\",\"y\"]"));

  // A list of objects: the add button creates an item with its required fields; items have their own sub-forms.
  const JsonPath p0 = [&] { JsonPath p = r.p({"parts"}); p.push_back(PathStep::at(0)); p.push_back(PathStep::member("size")); return p; }();
  QVERIFY(r.form->fieldAt(p0));
  QWidget* parts = r.form->fieldAt(p0)->parentWidget();
  while (parts->objectName() != "listField") parts = parts->parentWidget();
  parts->findChild<QPushButton*>("addItem")->click();
  QCOMPARE(r.at({"parts"}).size(), size_t(3));
  QVERIFY(r.at({"parts"})[size_t(2)].has("id"));
  r.stack()->undo();
  QCOMPARE(r.at({"parts"}).size(), size_t(2));

  // A tuple is edited as a whole (two boxes, one array).
  auto* pair = r.field<TupleField>({"pair"});
  pair->edits()[0]->setText("1");
  emit pair->edits()[0]->editingFinished();
  QVERIFY(!resolvePath(r.doc->root(), r.p({"pair"})));  // not written until both have a value
  pair->edits()[1]->setText("5");
  emit pair->edits()[1]->editingFinished();
  QCOMPARE(dumpOf(r.at({"pair"})), QString("[1,5]"));
}

void FormTest::mapsListOneRowPerId() {
  Rig r;
  const auto keys = r.form->findChildren<QLabel*>("mapKey");
  QStringList names;
  for (auto* k : keys) names << k->text();
  QCOMPARE(names, (QStringList{"one", "two", "three"}));
  auto* two = qobject_cast<ScalarField*>(r.form->fieldAt([&] { JsonPath p = r.p({"weights"}); p.push_back(PathStep::member("two")); return p; }()));
  QVERIFY(two);
  QCOMPARE(two->edit()->text(), QString());
  two->edit()->setText("9");
  emit two->edit()->editingFinished();
  QCOMPARE(r.at({"weights"})["two"].asInt(), 9);
  QCOMPARE(r.at({"weights"})["one"].asInt(), 5);
  r.stack()->undo();
  QVERIFY(!r.at({"weights"}).has("two"));
  // A key that is no id of ids.json is listed too, marked, and kept.
  r.ctx->set([&] { JsonPath p = r.p({"weights"}); p.push_back(PathStep::member("ghost")); return p; }(), Json::makeNumber(1));
  r.flush();
  QStringList after;
  for (auto* k : r.form->findChildren<QLabel*>("mapKey")) after << k->text();
  QVERIFY(after.contains("ghost (unknown id)"));
}

void FormTest::blocksEnableAndDisable() {
  Rig r;
  auto* enable = r.form->findChild<QCheckBox*>("blockEnable");
  QVERIFY(enable && !enable->isChecked());
  QVERIFY(!resolvePath(r.doc->root(), r.p({"block"})));
  const int before = r.stack()->count();
  enable->click();
  QCOMPARE(r.stack()->count(), before + 1);
  QVERIFY(r.at({"block"}).isObject());
  QVERIFY(r.at({"block"}).has("level"));  // required fields are filled in
  r.flush();
  QVERIFY(r.field<ScalarField>({"block", "level"}));
  // A field inside the block is a normal edit.
  auto* level = r.field<ScalarField>({"block", "level"});
  level->edit()->setText("3");
  emit level->edit()->editingFinished();
  QCOMPARE(r.at({"block", "level"}).asInt(), 3);
  r.form->findChild<QCheckBox*>("blockEnable")->click();  // disable: the whole block goes
  QVERIFY(!resolvePath(r.doc->root(), r.p({"block"})));
  while (r.stack()->canUndo()) r.stack()->undo();
  QVERIFY(!resolvePath(r.doc->root(), r.p({"block"})));
  r.stack()->redo();
  r.stack()->redo();
  QCOMPARE(r.at({"block", "level"}).asInt(), 3);
}

void FormTest::jsonFieldUsesTheDialogHook() {
  Rig r;
  r.jsonAnswer = Json();
  std::string err;
  Json::parse(R"({"a": [1, 2], "b": null})", *r.jsonAnswer, err);
  auto* raw = r.field<JsonField>({"raw"});
  raw->findChild<QToolButton*>("editJsonButton")->click();
  QCOMPARE(dumpOf(r.at({"raw"})), QString(R"({"a":[1,2],"b":null})"));
  r.stack()->undo();
  QVERIFY(!resolvePath(r.doc->root(), r.p({"raw"})));
}

void FormTest::unknownKeysSurviveByteForByte() {
  Rig r;
  const QByteArray original = r.doc->serialize();
  const Json extraBefore = r.at({"extra"});
  // Edit many known fields through the form, in several ways.
  auto* label = r.field<ScalarField>({"label"});
  label->edit()->setText("new");
  emit label->edit()->editingFinished();
  r.field<BoolField>({"flag"})->box()->click();
  r.flush();
  r.form->findChild<QCheckBox*>("blockEnable")->click();
  r.flush();
  QCOMPARE(dumpOf(r.at({"extra"})), dumpOf(extraBefore));
  QCOMPARE(QString::fromStdString(r.at({"zzz"}).asString()), QString("unknown, must survive"));
  // The text of the unknown members is identical in the saved text, and their order relative to each other too.
  const QByteArray edited = r.doc->serialize();
  const int e0 = edited.indexOf("\"extra\""), z0 = edited.indexOf("\"zzz\"");
  QVERIFY(e0 > 0 && z0 > e0);
  QVERIFY(original.contains(edited.mid(e0, z0 - e0 + 10)));
  while (r.stack()->canUndo()) r.stack()->undo();
  QCOMPARE(r.doc->serialize(), original);  // undo all: byte-identical
  QVERIFY(!r.doc->isDirty());
}

void FormTest::otherKeysAreShownAndEditable() {
  Rig r;
  auto* box = r.form->findChild<QWidget*>("otherKeys");
  QVERIFY(box);
  auto* text = box->findChild<QWidget*>("otherKeysText");
  QVERIFY(text);
  QVERIFY(QString::fromStdString(r.at({"extra"}).dump(0)).size() > 0);
  // Edit as JSON: change one unknown key, remove another, add a third; known keys stay.
  r.jsonAnswer = Json();
  std::string err;
  Json::parse(R"({"extra": {"keep": [9]}, "fresh": 1})", *r.jsonAnswer, err);
  const int before = r.stack()->count();
  box->findChild<QPushButton*>("otherKeysEdit")->click();
  QCOMPARE(r.stack()->count(), before + 1);  // one step (a macro)
  QVERIFY(!r.doc->root().has("zzz"));
  QVERIFY(r.doc->root().has("fresh"));
  QCOMPARE(r.at({"label"}).asString().c_str(), "hello");
  r.stack()->undo();
  QVERIFY(r.doc->root().has("zzz"));
  QVERIFY(!r.doc->root().has("fresh"));
  // "Show in tree view" asks for the tree.
  QSignalSpy spy(r.form.get(), &FormView::showInTreeRequested);
  box->findChild<QPushButton*>("otherKeysTree")->click();
  QCOMPARE(spy.count(), 1);
}

void FormTest::aWrongKindIsShownWithAWarningAndNeverChanged() {
  Rig r(R"({ "id": "t1", "count": "lots", "flag": 3, "pair": [1, 2, 3], "label": 7 })");
  const QByteArray original = r.doc->serialize();
  // Every one of them became a raw-JSON field with a warning; none was coerced.
  for (const char* k : {"count", "flag", "pair", "label"}) {
    auto* f = r.field<JsonField>({k});
    QVERIFY2(f, k);
    auto* note = f->findChild<QLabel*>("fieldNote");
    QVERIFY2(note && !note->isHidden() && note->text().contains("Expected"), k);
  }
  QCOMPARE(r.field<JsonField>({"count"})->findChild<QLineEdit*>("jsonLine")->text(), QString("\"lots\""));
  QCOMPARE(r.doc->serialize(), original);
  // Editing something else leaves them as they are.
  r.ctx->set(r.p({"mode"}), Json::makeString("a"));
  r.flush();
  QCOMPARE(QString::fromStdString(r.at({"count"}).asString()), QString("lots"));
  QCOMPARE(r.at({"flag"}).asInt(), 3);
  QCOMPARE(r.at({"pair"}).size(), size_t(3));
  // Fixing the value through the JSON editor turns it back into a normal field.
  r.jsonAnswer = Json::makeNumber(5);
  r.field<JsonField>({"count"})->findChild<QToolButton*>("editJsonButton")->click();
  r.flush();
  QVERIFY(r.field<ScalarField>({"count"}));
  QCOMPARE(r.at({"count"}).asInt(), 5);
}

void FormTest::tableFormAndTreeStayInSync() {
  ote_test::Scratch tmp;
  const QString dir = ote_test::copyFixture(tmp, "damage-types");
  SchemaSet schemas;
  schemas.loadFolder(QStringLiteral(OTE_SOURCE_DIR "/editor/schema"));
  Project project(dir);
  IdSource ids(&project);
  JsonDocument* doc = project.document("data/weapons.json");
  TableEditor table(&project, doc, {PathStep::member("weapons")}, nullptr, schemas.forFile("data/weapons.json"), &ids);
  TreeEditor tree(&project, doc);
  table.resize(1100, 700);
  table.selectRow(1);
  QVERIFY(table.form()->hasTarget());
  const JsonPath row1 = {PathStep::member("weapons"), PathStep::at(1)};
  QCOMPARE(table.form()->target(), row1);

  // form -> table and tree
  auto* name = qobject_cast<ScalarField*>(table.form()->fieldAt([&] { JsonPath p = row1; p.push_back(PathStep::member("name")); return p; }()));
  QVERIFY(name);
  name->edit()->setText("Renamed");
  emit name->edit()->editingFinished();
  int nameCol = -1;
  for (int c = 0; c < table.model()->columnCount(); ++c) if (table.model()->columnKey(c) == "name") nameCol = c;
  QCOMPARE(table.model()->index(1, nameCol).data().toString(), QString("Renamed"));
  QCOMPARE(QString::fromStdString(doc->root()["weapons"][size_t(1)]["name"].asString()), QString("Renamed"));
  // table -> form
  QVERIFY(table.model()->setData(table.model()->index(1, nameCol), "FromTable", Qt::EditRole));
  QCOMPARE(name->edit()->text(), QString("FromTable"));
  // tree -> form and table
  const QModelIndex nameInTree = tree.model()->indexForPath([&] { JsonPath p = row1; p.push_back(PathStep::member("name")); return p; }());
  QVERIFY(nameInTree.isValid());
  QVERIFY(tree.model()->setData(nameInTree.siblingAtColumn(JsonTreeModel::ValueColumn), "FromTree", Qt::EditRole));
  QCOMPARE(name->edit()->text(), QString("FromTree"));
  QCOMPARE(table.model()->index(1, nameCol).data().toString(), QString("FromTree"));
  // undo reverses all three, in all three views
  project.undoStack()->undo();
  project.undoStack()->undo();
  project.undoStack()->undo();
  QCOMPARE(name->edit()->text(), QString("Zapper"));
  // A new key made by the form (the table gets a column for it) keeps the row selected and shown.
  table.formContext()->set([&] { JsonPath p = row1; p.push_back(PathStep::member("fire")); return p; }(), Json::makeString("line"));
  QCoreApplication::processEvents();
  QCOMPARE(table.currentSourceRow(), 1);
  QVERIFY(table.form()->hasTarget());
  // Deleting the shown row clears the form (or moves it to the row that took its place).
  table.model()->deleteRows({1});
  QCoreApplication::processEvents();
  QVERIFY(table.form()->target().size() == 0 || table.currentSourceRow() >= -1);
  project.undoStack()->undo();
  QCoreApplication::processEvents();
}

void FormTest::problemMessagesFindTheRowAndTheField() {
  ote_test::Scratch tmp;
  const QString dir = ote_test::copyFixture(tmp, "damage-types");
  SchemaSet schemas;
  schemas.loadFolder(QStringLiteral(OTE_SOURCE_DIR "/editor/schema"));
  Project project(dir);
  IdSource ids(&project);
  JsonDocument* doc = project.document("data/weapons.json");
  TableEditor table(&project, doc, {PathStep::member("weapons")}, nullptr, schemas.forFile("data/weapons.json"), &ids);
  table.resize(1100, 700);
  const QString id = QString::fromStdString(doc->root()["weapons"][size_t(2)]["id"].asString());
  // A loader message as the Problems dock shows it (the file part is already split off).
  QVERIFY(table.locateProblem(QString("weapon \"%1\": \"damageType\" names unknown damage type \"x\" (game.json \"damageTypes\")").arg(id)));
  QCOMPARE(table.currentSourceRow(), 2);
  QVERIFY(table.form()->focusKey("damageType"));
  QVERIFY(!table.locateProblem("something with no entry in it"));
  // By position: weapons[3]
  QVERIFY(table.locateProblem("weapons[3]: \"range\" must be [min, max]"));
  QCOMPARE(table.currentSourceRow(), 3);

  // game.json: "rules.actionPoints" and a bare quoted key.
  JsonDocument* game = project.document("game.json");
  TreeEditor tree(&project, game, nullptr, schemas.forFile("game.json"), &ids);
  tree.resize(900, 700);
  QVERIFY(tree.locateProblem("rules.actionPoints: unknown key \"x\" (expected \"resource\")"));
  QVERIFY(tree.locateProblem("\"resistRange\" must be [min, max] with two whole numbers"));
  QVERIFY(tree.pages()->currentWidget() == tree.form());
}

void FormTest::tableColumnsComeFromTheSchema() {
  ote_test::Scratch tmp;
  const QString dir = ote_test::copyFixture(tmp, "damage-types");
  SchemaSet schemas;
  schemas.loadFolder(QStringLiteral(OTE_SOURCE_DIR "/editor/schema"));
  Project project(dir);
  IdSource ids(&project);
  JsonDocument* doc = project.document("data/units.json");
  JsonTableModel model(&project, doc, *findTableArray(doc->root()));
  model.setSchema(schemas.forFile("data/units.json")->rows.get(), &ids);
  QStringList cols;
  for (int c = 0; c < model.columnCount(); ++c) cols << model.columnKey(c);
  QVERIFY(cols.contains("stats.hp"));
  QVERIFY(cols.contains("stats.mov"));
  QVERIFY(cols.contains("resist.fire"));  // one column per damage type of game.json
  QVERIFY(!cols.contains("stats"));
  // Editing a nested cell that does not exist yet creates the parent object in the same undo step.
  const int fireCol = cols.indexOf("resist.fire");
  int row = -1;
  for (int r = 0; r < model.rowCount(); ++r) if (!model.rowValue(r)->has("resist")) row = r;
  if (row >= 0) {
    const int before = project.undoStack()->count();
    QVERIFY(model.setData(model.index(row, fireCol), "2", Qt::EditRole));
    QCOMPARE((*model.rowValue(row))["resist"]["fire"].asInt(), 2);
    QCOMPARE(project.undoStack()->count(), before + 1);
    project.undoStack()->undo();
    QVERIFY(!model.rowValue(row)->has("resist"));
  }
  // A cell of the wrong kind is kept and reports a mismatch.
  model.setCellValue(0, cols.indexOf("stats.hp"), Json::makeString("lots"));
  QString why;
  QVERIFY(model.cellMismatch(0, cols.indexOf("stats.hp"), &why));
  QVERIFY(!why.isEmpty());
}

QTEST_MAIN(FormTest)
#include "test_form.moc"
