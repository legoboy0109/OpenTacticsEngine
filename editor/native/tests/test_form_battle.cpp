// The battle scene's art forms (data/graphics.json "battle", the "either one cell or a list" values of join and stand variants) with
// the real schemas on a copy of tests/fixtures/battle-art, which holds every key of the loader's battle section: the form shows all
// of it without a warning mark and without anything under "Other keys", and edits write exactly the JSON the loader reads.
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QToolButton>
#include <QUndoStack>
#include <QtTest>
#include <memory>

#include "form_context.h"
#include "form_fields.h"
#include "form_view.h"
#include "id_source.h"
#include "project.h"
#include "test_support.h"
#include "validate.h"

using namespace ote_native;
using ote::Json;

namespace {

Json parse(const QString& text) {
  Json j;
  std::string err;
  if (!Json::parse(text.toStdString(), j, err)) qFatal("bad JSON in the test: %s", err.c_str());
  return j;
}

JsonPath path(const QString& text) {  // "battle.effects[0].frames[1]"
  JsonPath out;
  for (const QString& part : text.split('.')) {
    const int br = part.indexOf('[');
    if (br < 0) { out.push_back(PathStep::member(part.toStdString())); continue; }
    if (br > 0) out.push_back(PathStep::member(part.left(br).toStdString()));
    out.push_back(PathStep::at(part.mid(br + 1, part.size() - br - 2).toUInt()));
  }
  return out;
}

struct Rig {
  ote_test::Scratch tmp;
  SchemaSet schemas;
  QString dir;
  std::unique_ptr<Project> project;
  std::unique_ptr<IdSource> ids;
  JsonDocument* doc = nullptr;
  std::unique_ptr<FormContext> ctx;
  std::unique_ptr<FormView> form;

  explicit Rig(const char* fixture = "battle-art", const char* file = "data/graphics.json") {
    dir = ote_test::copyFixture(tmp, fixture);
    schemas.loadFolder(QStringLiteral(OTE_SOURCE_DIR "/editor/schema"));
    if (!schemas.errors().isEmpty()) qFatal("schemas: %s", qPrintable(schemas.errors().join("\n")));
    project = std::make_unique<Project>(dir);
    ids = std::make_unique<IdSource>(project.get());
    doc = project->document(file);
    ctx = std::make_unique<FormContext>(project.get(), doc, ids.get());
    form = std::make_unique<FormView>(ctx.get(), schemas.forFile(file)->root.get());
    form->resize(1100, 800);
    form->setTarget({});
    flush();
  }
  void flush() { form->flush(); QCoreApplication::processEvents(); form->flush(); }
  const Json* at(const QString& p) { return resolvePath(doc->root(), path(p)); }
  QString text(const QString& p) { const Json* j = at(p); return j ? QString::fromStdString(j->dump(0)) : QStringLiteral("(absent)"); }
  QWidget* anchor(const QString& p) { form->expandAlong(path(p)); flush(); return form->anchorWidget(path(p)); }
  template <class T> T* field(const QString& p) { form->expandAlong(path(p)); flush(); return qobject_cast<T*>(form->fieldAt(path(p))); }
  QUndoStack* stack() { return project->undoStack(); }
  void setText(const QString& p, const QString& value) {
    auto* f = field<ScalarField>(p);
    QVERIFY2(f, qPrintable("no number or text field at " + p));
    f->edit()->setText(value);
    emit f->edit()->editingFinished();
  }
  // Picks the entry of the variant picker (the combo of the value at `p`) that has this name.
  void pickVariant(const QString& p, const QString& name) {
    QWidget* a = anchor(p);
    QVERIFY2(a, qPrintable("no variant at " + p));
    auto* combo = a->findChild<QComboBox*>("variantCombo");
    QVERIFY2(combo, qPrintable("no variant combo at " + p));
    const int i = combo->findText(name);
    QVERIFY2(i >= 0, qPrintable(name + " is not offered at " + p));
    combo->setCurrentIndex(i);
    emit combo->activated(i);
    flush();
  }
  QString variantShown(const QString& p) {
    QWidget* a = anchor(p);
    auto* combo = a ? a->findChild<QComboBox*>("variantCombo") : nullptr;
    return combo ? combo->currentText() : QStringLiteral("(no combo)");
  }
};

QStringList warningsOf(FormView* f) {
  QStringList out;
  for (QLabel* l : f->findChildren<QLabel*>())
    if (l->isVisibleTo(f) && !l->text().isEmpty() &&
        (l->objectName() == "fieldNote" || l->objectName() == "notApplicable" || l->objectName() == "formMessage" || l->text().contains("(unknown id)")))
      out << l->text();
  return out;
}

QStringList otherKeysOf(FormView* f) {
  QStringList keys;
  for (QPlainTextEdit* t : f->findChildren<QPlainTextEdit*>("otherKeysText")) {
    Json j;
    std::string err;
    if (Json::parse(t->toPlainText().toStdString(), j, err))
      for (const auto& m : j.members()) keys << QString::fromStdString(m.first);
  }
  return keys;
}

}  // namespace

class FormBattleTest : public QObject {
  Q_OBJECT
 private slots:
  void theFixtureIsAValidGame();
  void everyBattleKeyHasAFormWithoutWarningOrOtherKeys();
  void numbersAndWordsWriteExactJson();
  void blocksAndTimesAndFramesWriteExactJson();
  void assignmentsAndEffectListsWriteExactJson();
  void joinAndStandVariantsAreRealForms();
  void aWrongValueIsKeptAndMarked();
};

void FormBattleTest::theFixtureIsAValidGame() {
  Rig r;
  const auto problems = ote_editor::validateProject(r.dir.toStdString());
  QStringList text;
  for (const auto& p : problems) text << QString::fromStdString(p.file + ": " + p.message);
  QVERIFY2(problems.empty(), qPrintable(text.join("\n")));
}

void FormBattleTest::everyBattleKeyHasAFormWithoutWarningOrOtherKeys() {
  Rig r;
  r.form->expandAll();
  r.flush();
  QCOMPARE(warningsOf(r.form.get()), QStringList());
  QCOMPARE(otherKeysOf(r.form.get()), QStringList());
  // Nothing is left as raw JSON but the one null animation the loader accepts as "none".
  QStringList raw;
  for (QLineEdit* e : r.form->findChildren<QLineEdit*>("jsonLine")) raw << e->text();
  QCOMPARE(raw, QStringList{"null"});
  // A few places that used to be raw JSON are forms now.
  QVERIFY(r.form->hasAnchor(path("battle.sets.hero_set.animations.attack")));
  QVERIFY(r.form->hasAnchor(path("battle.effects[0].hide[0]")));
  QVERIFY(r.form->hasAnchor(path("battle.stage.ranges[1].pan")));
  QVERIFY(r.field<ScalarField>("battle.stage.width"));
  QVERIFY(r.field<ChoiceField>("battle.stage.fit"));
  QVERIFY(r.field<TupleField>("battle.stage.feet.left"));
  // A frame of another sheet is a three-part value: two numbers and the sheet.
  auto* three = r.field<TupleField>("battle.effects[0].frames[1]");
  QVERIFY(three);
  QCOMPARE(three->edits().size(), 3);
  QCOMPARE(three->edits()[2]->text(), QString("units"));
  // The loader is the truth: the fixture the form shows is a valid game, and nothing above changed it.
  QVERIFY(!r.doc->isDirty());
}

void FormBattleTest::numbersAndWordsWriteExactJson() {
  Rig r;
  r.setText("battle.stage.width", "320");
  QCOMPARE(r.text("battle.stage.width"), QString("320"));
  QCOMPARE(r.stack()->count(), 1);
  r.setText("battle.stage.height", "999");
  r.setText("battle.scene.resultHoldMs", "0");
  QCOMPARE(r.text("battle.scene.resultHoldMs"), QString("0"));
  // A number outside the range is refused and nothing is written.
  r.setText("battle.stage.width", "5000");
  QCOMPARE(r.text("battle.stage.width"), QString("320"));
  QCOMPARE(r.stack()->count(), 3);
  // An enum word and a yes / no.
  auto* fit = r.field<ChoiceField>("battle.stage.fit");
  QVERIFY(fit);
  fit->combo()->setCurrentIndex(fit->combo()->findData("cover"));
  emit fit->combo()->activated(fit->combo()->currentIndex());
  QCOMPARE(r.text("battle.stage.fit"), QString("\"cover\""));
  auto* line = r.field<BoolField>("battle.stage.rangeLine");
  QVERIFY(line);
  line->box()->click();
  QCOMPARE(r.text("battle.stage.rangeLine"), QString("false"));
  // A point of the stage (two numbers) and a sound by frame number.
  auto* feet = r.field<TupleField>("battle.stage.feet.left");
  feet->edits()[0]->setText("70");
  emit feet->edits()[0]->editingFinished();
  QCOMPARE(r.text("battle.stage.feet.left"), QString("[70,120]"));
  // One undo step each, and undoing all of them gives the file back byte for byte.
  while (r.stack()->canUndo()) r.stack()->undo();
  QVERIFY(!r.doc->isDirty());
  QCOMPARE(r.text("battle.stage.width"), QString("240"));
  QCOMPARE(r.text("battle.stage.feet.left"), QString("[60,120]"));
}

void FormBattleTest::blocksAndTimesAndFramesWriteExactJson() {
  Rig r;
  // The other animation slots are optional blocks with an Enable box: enabling writes a sheet and one frame the loader accepts.
  const QString cast = "battle.sets.plain_set.animations.cast";
  QWidget* block = r.anchor("battle.sets.plain_set.animations");
  QVERIFY(block);
  QVERIFY(!r.at(cast));
  QWidget* castBlock = nullptr;
  for (QCheckBox* b : block->findChildren<QCheckBox*>("blockEnable"))
    if (b->text() == "cast") castBlock = b;
  QVERIFY(castBlock);
  static_cast<QCheckBox*>(castBlock)->click();
  r.flush();
  QCOMPARE(r.text(cast), QString("{\"sheet\":\"units\",\"frames\":[[0,0]]}"));
  QVERIFY(r.stack()->count() == 1);
  // The time of a frame: one number for every frame, or one per frame; switching writes the picker's starting value.
  const QString ms = "battle.sets.hero_set.animations.attack.ms";
  QCOMPARE(r.text(ms), QString("[140,90,110]"));
  QCOMPARE(r.variantShown(ms), QString("one time per frame"));
  r.pickVariant(ms, "same for every frame");
  QCOMPARE(r.text(ms), QString("120"));
  QCOMPARE(r.variantShown(ms), QString("same for every frame"));
  r.setText(ms, "200");
  QCOMPARE(r.text(ms), QString("200"));
  r.pickVariant(ms, "one time per frame");
  QCOMPARE(r.text(ms), QString("[120]"));
  // Frames of an effect: a cell of its own sheet, or [column, row, "sheet"].
  QCOMPARE(r.variantShown("battle.effects[0].frames[0]"), QString("cell"));
  QCOMPARE(r.variantShown("battle.effects[0].frames[1]"), QString("cell of another sheet"));
  auto* three = r.field<TupleField>("battle.effects[0].frames[1]");
  three->edits()[2]->setText("red");
  emit three->edits()[2]->editingFinished();
  QCOMPARE(r.text("battle.effects[0].frames[1]"), QString("[1,0,\"red\"]"));
  r.pickVariant("battle.effects[0].frames[1]", "cell");
  QCOMPARE(r.text("battle.effects[0].frames[1]"), QString("[0,0]"));
  // A step of a list of timed changes: add one to `shake`, fill it in.
  QCOMPARE(r.text("battle.effects[0].shake"), QString("[{\"atMs\":50,\"ms\":200,\"strength\":3,\"atRangeFixed\":true}]"));
  r.setText("battle.effects[0].shake[0].strength", "9");
  QCOMPARE(r.text("battle.effects[0].shake[0]"), QString("{\"atMs\":50,\"ms\":200,\"strength\":9,\"atRangeFixed\":true}"));
  // The camera of a panning band appears (as a required object) only with view "each".
  QVERIFY(r.at("battle.stage.ranges[1].pan"));
  r.setText("battle.stage.ranges[1].pan.panMs", "250");
  QCOMPARE(r.text("battle.stage.ranges[1].pan.panMs"), QString("250"));
}

void FormBattleTest::assignmentsAndEffectListsWriteExactJson() {
  Rig r;
  // A rule's set comes from the sets the file has.
  auto* set = r.field<ChoiceField>("battle.assign[0].set");
  QVERIFY(set);
  QStringList words;
  for (int i = 0; i < set->combo()->count(); ++i) words << set->combo()->itemData(i).toString();
  QVERIFY(words.contains("hero_set") && words.contains("plain_set"));
  set->combo()->setCurrentIndex(set->combo()->findData("plain_set"));
  emit set->combo()->activated(set->combo()->currentIndex());
  QCOMPARE(r.text("battle.assign[0]"), QString("{\"set\":\"plain_set\",\"character\":\"hero\"}"));
  // `effects` of an entry: one library effect, or a list of them.
  QCOMPARE(r.variantShown("battle.effects[4].effects"), QString("several effects"));
  QCOMPARE(r.variantShown("battle.effects[5].effects"), QString("one effect"));
  r.pickVariant("battle.effects[4].effects", "one effect");
  QCOMPARE(r.text("battle.effects[4].effects"), QString("\"spark\""));
  r.pickVariant("battle.effects[4].effects", "several effects");
  QCOMPARE(r.text("battle.effects[4].effects"), QString("[]"));
  // `byType` has a row for every unit type of the game.
  QWidget* by = r.anchor("battle.effects[4].byType");
  QVERIFY(by);
  QVERIFY(r.at("battle.effects[4].byType.archer"));
  QCOMPARE(r.variantShown("battle.effects[4].byType.fighter"), QString("several effects"));
  QCOMPARE(r.variantShown("battle.effects[4].byType.archer"), QString("one effect"));
}

void FormBattleTest::joinAndStandVariantsAreRealForms() {
  Rig r;
  r.ctx->set(path("terrain"), parse(R"({"plain":{"sheet":"units","join":[0,0]}})"));
  r.ctx->set(path("styles"), parse(R"({"classic":{"terrain":{"plain":{"sheet":"units","cell":[0,0],"stand":{"sheet":"units","variants":[[0,0],[1,0]]}}}}})"));
  r.flush();
  QCOMPARE(r.variantShown("terrain.plain.join"), QString("one block"));
  QVERIFY(r.field<TupleField>("terrain.plain.join"));
  r.pickVariant("terrain.plain.join", "animation (several blocks)");
  QCOMPARE(r.text("terrain.plain.join"), QString("[[0,2]]"));
  QCOMPARE(r.variantShown("terrain.plain.join"), QString("animation (several blocks)"));
  r.pickVariant("terrain.plain.join", "one block");
  QCOMPARE(r.text("terrain.plain.join"), QString("[0,2]"));
  // Choosing a join where there was a plain cell replaces the cell in the same undo step.
  r.ctx->set(path("terrain"), parse(R"({"plain":{"sheet":"units","cell":[0,0]}})"));
  r.flush();
  const int before = r.stack()->count();
  r.pickVariant("terrain.plain.join", "one block");
  QCOMPARE(r.text("terrain.plain"), QString("{\"sheet\":\"units\",\"join\":[0,2]}"));
  QCOMPARE(r.stack()->count(), before + 1);
  r.stack()->undo();
  QCOMPARE(r.text("terrain.plain"), QString("{\"sheet\":\"units\",\"cell\":[0,0]}"));
  // A standing thing with several pictures, or several model files.
  const QString v = "styles.classic.terrain.plain.stand.variants";
  QCOMPARE(r.variantShown(v), QString("cells of the sheet"));
  r.ctx->set(path("styles.classic.terrain.plain.stand"), parse(R"({"variants":["models/a.glb","models/b.glb"]})"));
  r.flush();
  QCOMPARE(r.variantShown(v), QString("model files"));
  r.pickVariant(v, "cells of the sheet");
  QCOMPARE(r.text(v), QString("[[0,0]]"));
  QStringList raw;
  for (QLineEdit* e : r.form->findChildren<QLineEdit*>("jsonLine")) raw << e->text();
  QVERIFY2(!raw.contains("[[0,0]]"), "the cells of a stand are a form, not raw JSON");
}

void FormBattleTest::aWrongValueIsKeptAndMarked() {
  Rig r;
  r.ctx->set(path("battle.stage.width"), Json::makeString("wide"));
  r.ctx->set(path("battle.scene.resultHoldMs"), Json::makeNumber(9000));
  r.ctx->set(path("battle.effects[0].layer"), Json::makeString("sideways"));
  r.flush();
  r.form->expandAll();
  r.flush();
  const QStringList w = warningsOf(r.form.get());
  QVERIFY2(w.size() >= 3, qPrintable(w.join(" | ")));
  QCOMPARE(r.text("battle.stage.width"), QString("\"wide\""));
  QCOMPARE(r.text("battle.scene.resultHoldMs"), QString("9000"));
  QCOMPARE(r.text("battle.effects[0].layer"), QString("\"sideways\""));
}

QTEST_MAIN(FormBattleTest)
#include "test_form_battle.moc"
