// The form features added in milestone 3: fields that apply only for some `kind` (`when`), one-of-several shapes
// (`variant`), maps with free names, blocks that need something first (`needs`) or write a given value (`enable`), lazy
// building (collapsed containers are not instantiated), two columns when wide, and the table | table + form | form views.
// All with small inline schemas: these tests are about the mechanisms, the real schemas are checked by the conformance test.
#include <QCheckBox>
#include <QComboBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QToolButton>
#include <QUndoStack>
#include <QtTest>
#include <memory>

#include "app_settings.h"
#include "form_context.h"
#include "form_fields.h"
#include "form_view.h"
#include "id_source.h"
#include "project.h"
#include "table_editor.h"
#include "test_support.h"

using namespace ote_native;
using ote::Json;

namespace {

const char* kSchemaText = R"({
  "files": ["data/thing.json"],
  "root": { "type": "object", "fields": {
    "id": { "type": "string", "required": true },
    "kind": { "type": "enum", "required": true, "values": ["usable", "weapon", "staff"] },
    "heal": { "type": "int", "min": 1, "when": { "kind": ["usable", "staff"] }, "help": "only for usable and staff" },
    "might": { "type": "int", "min": 1, "when": { "kind": "weapon" }, "help": "only for weapons" },
    "mode": { "type": "enum", "values": ["x", "y"], "default": "x" },
    "swing": { "type": "int", "when": { "mode": "y" } },
    "cond": { "type": "variant", "variants": [
      { "name": "hp", "key": "hp", "node": { "type": "object", "fields": {
          "hp": { "type": "object", "required": true, "fields": {
            "op": { "type": "enum", "required": true, "values": ["<", ">"] }, "percent": { "type": "int", "required": true } } },
          "who": { "type": "enum", "values": ["self", "foe"] } } } },
      { "name": "near", "key": "near", "node": { "type": "object", "fields": {
          "near": { "type": "int", "required": true }, "who": { "type": "enum", "values": ["self", "foe"] } } } },
      { "name": "plain id", "kind": "string", "node": { "type": "string" } },
      { "name": "always", "value": "always" } ] },
    "conds": { "type": "list", "of": { "type": "variant", "variants": [
      { "name": "near", "key": "near", "node": { "type": "object", "fields": { "near": { "type": "int", "required": true } } } },
      { "name": "plain id", "kind": "string", "node": { "type": "string" } } ] } },
    "named": { "type": "map", "free": true, "of": { "type": "int", "min": 0 } },
    "sets": { "type": "map", "free": true, "of": { "type": "object", "fields": { "dmg": { "type": "int" } } } },
    "needsRes": { "type": "object", "block": true, "needs": { "ref": "data/ids.json", "hint": "needs an id: add one in ids.json" },
                  "fields": { "res": { "type": "ref", "ref": "data/ids.json", "required": true } } },
    "given": { "type": "object", "block": true, "enable": { "n": 7 }, "fields": { "n": { "type": "int" }, "m": { "type": "int", "required": true } } },
    "parts": { "type": "list", "of": { "type": "object", "fields": {
      "id": { "type": "string" }, "a": { "type": "int" }, "b": { "type": "int" }, "c": { "type": "int" }, "d": { "type": "int" },
      "e": { "type": "int" }, "f": { "type": "int" }, "g": { "type": "int" },
      "inner": { "type": "object", "block": true, "fields": { "x": { "type": "int" }, "y": { "type": "int" } } } } } }
  } }
})";

const char* kThing = R"({ "id": "t1", "kind": "usable", "heal": 5 })";

struct Rig {
  ote_test::Scratch tmp;
  SchemaSet schemas;
  std::unique_ptr<Project> project;
  std::unique_ptr<IdSource> ids;
  JsonDocument* doc = nullptr;
  std::unique_ptr<FormContext> ctx;
  std::unique_ptr<FormView> form;

  explicit Rig(const char* thing = kThing, bool withIds = true) {
    ote_test::writeFile(tmp.file("g/game.json"), "{}");
    ote_test::writeFile(tmp.file("g/data/thing.json"), thing);
    ote_test::writeFile(tmp.file("g/data/ids.json"), withIds ? R"({ "ids": [ { "id": "one" } ] })" : R"({ "ids": [] })");
    schemas.addFromText("thing.json", kSchemaText);
    if (!schemas.errors().isEmpty()) qFatal("schema: %s", qPrintable(schemas.errors().join("\n")));
    project = std::make_unique<Project>(tmp.file("g"));
    ids = std::make_unique<IdSource>(project.get());
    doc = project->document("data/thing.json");
    ctx = std::make_unique<FormContext>(project.get(), doc, ids.get());
    form = std::make_unique<FormView>(ctx.get(), schemas.forFile("data/thing.json")->root.get());
    form->resize(560, 900);
    form->setTarget({});
    flush();
  }
  JsonPath p(std::initializer_list<const char*> keys) {
    JsonPath out;
    for (const char* k : keys) out.push_back(PathStep::member(k));
    return out;
  }
  const Json* at(std::initializer_list<const char*> keys) { return resolvePath(doc->root(), p(keys)); }
  FieldBase* field(std::initializer_list<const char*> keys) { return form->fieldAt(p(keys)); }
  void flush() { form->flush(); QCoreApplication::processEvents(); form->flush(); }
  void set(std::initializer_list<const char*> keys, const char* json) {
    Json v;
    std::string err;
    Json::parse(json, v, err);
    ctx->set(p(keys), v);
    flush();
  }
  QUndoStack* stack() { return project->undoStack(); }
  template <class T> T* find(const char* name) { return form->body()->findChild<T*>(name); }
};

}  // namespace

class FormFeaturesTest : public QObject {
  Q_OBJECT
 private slots:
  void whenShowsTheFieldsOfTheKind();
  void aKeyThatDoesNotApplyIsStillShownWithANote();
  void variantPicksTheShapeAndSwitchingIsOneUndoStep();
  void variantKeepsSharedKeysAndShowsUnknownShapesAsJson();
  void variantItemsInAListAndConstantVariants();
  void freeMapAddsRemovesAndRefusesDuplicates();
  void aBlockThatNeedsSomethingSaysSoAndEnableWritesTheGivenValue();
  void collapsedContainersAreNotBuiltUntilOpened();
  void focusOpensCollapsedContainers();
  void twoColumnsWhenWide();
  void viewModesOfATableFile();
  void bigFormsOpenInstantly();
};

void FormFeaturesTest::whenShowsTheFieldsOfTheKind() {
  Rig r;
  QVERIFY(r.field({"heal"}));
  QVERIFY(!r.field({"might"}));  // a weapon-only key is not offered for a usable item
  r.set({"kind"}, "\"weapon\"");
  QVERIFY(r.field({"might"}));
  QVERIFY(r.at({"heal"}));  // never dropped from the file just because the kind changed ...
  QVERIFY(r.find<QLabel>("notApplicable"));  // ... it stays visible, with a note
  r.ctx->erase(r.p({"heal"}));
  r.flush();
  QVERIFY(!r.field({"heal"}));  // once it is gone the form does not offer it for a weapon
  // The default of the discriminating sibling counts when the key is absent.
  r.set({"mode"}, "\"y\"");
  QVERIFY(r.field({"swing"}));
  r.ctx->erase(r.p({"mode"}));
  r.flush();
  QVERIFY(!r.field({"swing"}));
}

void FormFeaturesTest::aKeyThatDoesNotApplyIsStillShownWithANote() {
  Rig r(R"({ "id": "t", "kind": "weapon", "heal": 3, "might": 2 })");
  QVERIFY(r.field({"heal"}));
  QVERIFY(r.field({"might"}));
  QLabel* note = r.find<QLabel>("notApplicable");
  QVERIFY(note);
  QVERIFY(note->text().contains("kind"));
}

void FormFeaturesTest::variantPicksTheShapeAndSwitchingIsOneUndoStep() {
  Rig r(R"({ "id": "t", "kind": "usable", "cond": { "near": 2, "who": "foe" } })");
  r.form->expandAll();
  auto* combo = r.find<QComboBox>("variantCombo");
  QVERIFY(combo);
  QCOMPARE(combo->currentText(), QString("near"));
  QVERIFY(r.field({"cond", "near"}));
  QVERIFY(!r.field({"cond", "hp", "op"}));
  const int before = r.stack()->index();
  combo->setCurrentIndex(combo->findText("hp"));
  emit combo->activated(combo->currentIndex());
  r.flush();
  QCOMPARE(r.stack()->index(), before + 1);
  QVERIFY(r.at({"cond", "hp"}));
  QVERIFY(!r.at({"cond", "near"}));
  r.form->expandAll();
  QVERIFY(r.field({"cond", "hp", "op"}));
  r.stack()->undo();
  r.flush();
  QVERIFY(r.at({"cond", "near"}));
  QVERIFY(!r.at({"cond", "hp"}));
}

void FormFeaturesTest::variantKeepsSharedKeysAndShowsUnknownShapesAsJson() {
  Rig r(R"({ "id": "t", "kind": "usable", "cond": { "near": 2, "who": "foe" } })");
  auto* combo = r.find<QComboBox>("variantCombo");
  combo->setCurrentIndex(combo->findText("hp"));
  emit combo->activated(combo->currentIndex());
  r.flush();
  QCOMPARE(QString::fromStdString((*r.at({"cond", "who"})).asString()), QString("foe"));  // `who` exists in both shapes: kept
  // A shape no variant describes stays as it is, with a warning, and is not converted.
  Rig u(R"({ "id": "t", "kind": "usable", "cond": { "mystery": 1 } })");
  QVERIFY(!u.find<QComboBox>("variantCombo"));
  auto* json = qobject_cast<JsonField*>(u.field({"cond"}));
  QVERIFY(json);
  QCOMPARE(QString::fromStdString(u.at({"cond"})->dump(0)), QString(R"({"mystery":1})"));
}

void FormFeaturesTest::variantItemsInAListAndConstantVariants() {
  Rig r(R"({ "id": "t", "kind": "usable", "cond": "always", "conds": [ "ab", { "near": 1 } ] })");
  r.form->expandAll();
  auto combos = r.form->body()->findChildren<QComboBox*>("variantCombo");
  QVERIFY(combos.size() >= 3);
  QStringList names;
  for (auto* c : combos) names << c->currentText();
  QVERIFY2(names.contains("always"), qPrintable(names.join("|")));  // constant variant: nothing more to show
  // the list items show their variant's name in the title
  QStringList titles;
  for (auto* l : r.form->body()->findChildren<QLabel*>("itemTitle")) titles << l->text();
  QVERIFY2(titles.join("|").contains("plain id") && titles.join("|").contains("near"), qPrintable(titles.join("|")));
  // adding an item starts as the first variant
  QPushButton* addCond = nullptr;
  for (auto* l : r.form->body()->findChildren<QWidget*>("listField"))
    if (l->property("path").toString() == "conds") addCond = l->findChild<QPushButton*>("addItem");
  QVERIFY(addCond);
  const size_t n = r.at({"conds"})->size();
  r.ctx->setVariantPicker([](const QString&, const std::vector<std::pair<QString, QString>>&) { return 0; });  // Add asks which kind
  addCond->click();
  r.flush();
  QCOMPARE(r.at({"conds"})->size(), n + 1);
}

void FormFeaturesTest::freeMapAddsRemovesAndRefusesDuplicates() {
  Rig r(R"({ "id": "t", "kind": "usable", "named": { "a": 1 } })");
  auto* name = r.form->body()->findChildren<QLineEdit*>("mapNewKey").value(0);
  auto* add = r.form->body()->findChildren<QPushButton*>("mapAdd").value(0);
  QVERIFY(name && add);
  name->setText("b");
  add->click();
  r.flush();
  QVERIFY(r.at({"named", "b"}));
  // a second add of the same name writes nothing and says so
  const int idx = r.stack()->index();
  r.form->body()->findChildren<QLineEdit*>("mapNewKey").value(0)->setText("b");
  r.form->body()->findChildren<QPushButton*>("mapAdd").value(0)->click();
  QCOMPARE(r.stack()->index(), idx);
  // remove
  auto rows = r.form->body()->findChildren<QToolButton*>("mapRemove");
  QVERIFY(rows.size() >= 2);
  rows[0]->click();
  r.flush();
  QVERIFY(!r.at({"named", "a"}));
  QVERIFY(r.at({"named", "b"}));
  // a map of objects: the new entry is an object
  r.form->body()->findChildren<QLineEdit*>("mapNewKey").value(1)->setText("set1");
  r.form->body()->findChildren<QPushButton*>("mapAdd").value(1)->click();
  r.flush();
  QVERIFY(r.at({"sets", "set1"}) && r.at({"sets", "set1"})->isObject());
}

void FormFeaturesTest::aBlockThatNeedsSomethingSaysSoAndEnableWritesTheGivenValue() {
  Rig none(kThing, /*withIds=*/false);
  none.form->expandAll();
  QVERIFY(none.find<QLabel>("blockNeeds"));
  QVERIFY(none.find<QLabel>("blockNeeds")->text().contains("add one in ids.json"));
  bool anyDisabled = false;
  for (auto* c : none.form->body()->findChildren<QCheckBox*>("blockEnable"))
    if (c->text() == "needsRes") anyDisabled = !c->isEnabled();
  QVERIFY(anyDisabled);
  Rig r;
  QVERIFY(!r.find<QLabel>("blockNeeds"));
  for (auto* c : r.form->body()->findChildren<QCheckBox*>("blockEnable")) {
    if (c->text() == "needsRes") { QVERIFY(c->isEnabled()); c->click(); }
  }
  r.flush();
  QCOMPARE(QString::fromStdString(r.at({"needsRes"})->dump(0)), QString(R"({"res":"one"})"));  // the first valid id filled in
  for (auto* c : r.form->body()->findChildren<QCheckBox*>("blockEnable"))
    if (c->text() == "given") c->click();
  r.flush();
  QCOMPARE(QString::fromStdString(r.at({"given"})->dump(0)), QString(R"({"n":7})"));  // exactly the schema's `enable` value
}

void FormFeaturesTest::collapsedContainersAreNotBuiltUntilOpened() {
  // 60 parts with 9 fields and a block each: far past the budget, so the later ones start collapsed.
  Json parts = Json::makeArray();
  for (int i = 0; i < 60; ++i) {
    Json o = Json::makeObject();
    o.set("id", Json::makeString("p" + std::to_string(i)));
    o.set("a", Json::makeNumber(i));
    o.set("inner", Json::makeObject());
    parts.push(o);
  }
  Json root = Json::makeObject();
  root.set("id", Json::makeString("t"));
  root.set("kind", Json::makeString("usable"));
  root.set("parts", parts);
  QElapsedTimer timer;
  Rig r(root.dump(0).c_str());
  QVERIFY2(r.form->collapsedCount() > 0, "nothing was collapsed");
  QVERIFY2(r.form->builtNodes() < 1200, qPrintable(QString::number(r.form->builtNodes())));
  const int collapsed = r.form->collapsedCount();
  // field of the last part is not instantiated yet ...
  JsonPath last = r.p({"parts"});
  last.push_back(PathStep::at(59));
  last.push_back(PathStep::member("a"));
  QVERIFY(!r.form->fieldAt(last));
  // ... opening its container builds it
  QVERIFY(r.form->expandAlong(last));
  QVERIFY(r.form->fieldAt(last));
  QVERIFY(r.form->collapsedCount() <= collapsed);
  // what the user opened stays open across a rebuild
  r.ctx->set(r.p({"label"}), Json::makeString("x"));
  r.flush();
  r.form->setTarget({});
  QVERIFY(r.form->fieldAt(last));
  // everything open: the whole form exists
  r.form->expandAll();
  QCOMPARE(r.form->collapsedCount(), 0);
  QVERIFY(r.form->builtNodes() > 0);
}

void FormFeaturesTest::focusOpensCollapsedContainers() {
  Json parts = Json::makeArray();
  for (int i = 0; i < 40; ++i) {
    Json o = Json::makeObject();
    o.set("id", Json::makeString("p" + std::to_string(i)));
    Json in = Json::makeObject();
    in.set("x", Json::makeNumber(1));
    o.set("inner", in);
    parts.push(o);
  }
  Json root = Json::makeObject();
  root.set("id", Json::makeString("t"));
  root.set("kind", Json::makeString("usable"));
  root.set("parts", parts);
  Rig r(root.dump(0).c_str());
  JsonPath p = r.p({"parts"});
  p.push_back(PathStep::at(39));
  p.push_back(PathStep::member("inner"));
  p.push_back(PathStep::member("x"));
  QVERIFY(!r.form->fieldAt(p));
  QVERIFY(r.form->focusPath(p));  // a problem names this key: the containers on the way open
  QVERIFY(r.form->fieldAt(p));
}

void FormFeaturesTest::twoColumnsWhenWide() {
  Rig r;
  r.form->resize(560, 900);
  r.flush();
  QCOMPARE(r.form->columns(), 1);
  auto xOf = [&](const char* key) {
    FieldBase* f = r.field({key});
    return f ? f->mapTo(r.form->body(), QPoint(0, 0)) : QPoint(-1, -1);
  };
  r.form->show();
  r.form->resize(1100, 900);
  QCoreApplication::processEvents();
  r.flush();
  QCOMPARE(r.form->columns(), 2);
  // id and kind sit side by side: same row, different x
  const QPoint a = xOf("id"), b = xOf("kind");
  r.form->body()->adjustSize();
  QVERIFY(a.x() >= 0 && b.x() >= 0);
  QVERIFY2(b.x() > a.x() + 100, qPrintable(QString("%1 %2").arg(a.x()).arg(b.x())));
  QVERIFY(qAbs(b.y() - a.y()) < 8);
  r.form->resize(700, 900);
  QCoreApplication::processEvents();
  r.flush();
  QCOMPARE(r.form->columns(), 1);
  r.form->hide();
}

void FormFeaturesTest::viewModesOfATableFile() {
  ote_test::Scratch tmp;
  ote_test::writeFile(tmp.file("g/game.json"), "{}");
  ote_test::writeFile(tmp.file("g/data/rows.json"), R"({ "rows": [ { "id": "a" }, { "id": "b" } ] })");
  SchemaSet schemas;
  schemas.addFromText("rows.json", R"({ "files": ["data/rows.json"], "rows": { "type": "object", "fields": { "id": { "type": "string" } } } })");
  Project project(tmp.file("g"));
  IdSource ids(&project);
  JsonDocument* doc = project.document("data/rows.json");
  TableEditor ed(&project, doc, {PathStep::member("rows")}, nullptr, schemas.forFile("data/rows.json"), &ids);
  ed.resize(1000, 600);
  ed.show();
  auto vis = [&](const char* name) { return ed.findChild<QWidget*>(name)->isVisible(); };
  QCOMPARE(ed.viewMode(), 1);
  QVERIFY(vis("tableView") && vis("formPane"));
  ed.findChild<QToolButton*>("viewMode0")->click();
  QCOMPARE(ed.viewMode(), 0);
  QVERIFY(vis("tableView") && !vis("formPane"));
  ed.findChild<QToolButton*>("viewMode2")->click();
  QCOMPARE(ed.viewMode(), 2);
  QVERIFY(!vis("tableView") && vis("formPane"));
  // form only: the arrows walk the rows
  ed.selectRow(0);
  QCOMPARE(ed.currentSourceRow(), 0);
  ed.findChild<QToolButton*>("formNextRow")->click();
  QCoreApplication::processEvents();
  QCOMPARE(ed.currentSourceRow(), 1);
  ed.findChild<QToolButton*>("formPrevRow")->click();
  QCOMPARE(ed.currentSourceRow(), 0);
  // remembered per kind of file
  QSettings qs(tmp.file("s.ini"), QSettings::IniFormat);
  AppSettings st(&qs);
  QCOMPARE(st.tableViewMode("units"), 1);  // default: table + form
  st.setTableViewMode("units", 2);
  st.setTableViewMode("weapons", 0);
  QCOMPARE(st.tableViewMode("units"), 2);
  QCOMPARE(st.tableViewMode("weapons"), 0);
  ed.hide();
}

// The real schemas: the Rules page with EVERY optional block switched on, and a skill with eight blocks of six effects each, must
// build in a blink (collapsed blocks are not instantiated). The bound is generous (a slow CI box); the measured time is printed.
void FormFeaturesTest::bigFormsOpenInstantly() {
  ote_test::Scratch tmp;
  ote_test::copyTree(QStringLiteral(OTE_SOURCE_DIR "/games/demo-srpg"), tmp.file("g"));
  SchemaSet real;
  real.loadFolder(QStringLiteral(OTE_SOURCE_DIR "/editor/schema"));
  QVERIFY2(real.errors().isEmpty(), qPrintable(real.errors().join("\n")));
  Project project(tmp.file("g"));
  IdSource ids(&project);
  JsonDocument* game = project.document("game.json");
  FormContext gctx(&project, game, &ids);
  const SchemaNode* rules = real.forFile("game.json")->root->field("rules");
  int enabled = 0;
  for (const auto& f : rules->fields)
    if (f.second->kind == SchemaNode::Kind::Object && f.second->block) {
      JsonPath p = {PathStep::member("rules"), PathStep::member(f.first)};
      if (!gctx.at(p)) gctx.set(p, defaultValue(*f.second, gctx.lookup()));
      ++enabled;
    }
  QVERIFY(enabled >= 15);
  FormView rulesForm(&gctx, real.forFile("game.json")->root.get());
  rulesForm.resize(1280, 800);
  QElapsedTimer timer;
  timer.start();
  rulesForm.setTarget({});
  rulesForm.flush();
  const qint64 rulesMs = timer.elapsed();
  qInfo("Rules page with %d blocks enabled: %lld ms, %d nodes built, %d containers collapsed", enabled, (long long)rulesMs, rulesForm.builtNodes(),
        rulesForm.collapsedCount());
  QVERIFY2(rulesMs < 1500, qPrintable(QString::number(rulesMs)));
  QVERIFY(rulesForm.collapsedCount() > 0 || rulesForm.builtNodes() < 400);

  // a skill with many blocks
  Json blocks = Json::makeArray();
  for (int i = 0; i < 8; ++i) {
    Json b = Json::makeObject();
    b.set("when", Json::makeString("combat"));
    Json fx = Json::makeArray();
    for (int k = 0; k < 6; ++k) {
      Json e = Json::makeObject();
      e.set("crit", Json::makeNumber(k + 1));
      fx.push(e);
    }
    b.set("do", fx);
    blocks.push(b);
  }
  JsonDocument* skills = project.document("data/skills.json");
  FormContext sctx(&project, skills, &ids);
  JsonPath first = {PathStep::member("skills"), PathStep::at(0), PathStep::member("blocks")};
  sctx.set(first, blocks);
  FormView skillForm(&sctx, real.forFile("data/skills.json")->rows.get());
  skillForm.resize(1280, 800);
  timer.restart();
  skillForm.setTarget({PathStep::member("skills"), PathStep::at(0)});
  skillForm.flush();
  const qint64 skillMs = timer.elapsed();
  qInfo("skill with 8 blocks x 6 effects: %lld ms, %d nodes built, %d containers collapsed", (long long)skillMs, skillForm.builtNodes(), skillForm.collapsedCount());
  QVERIFY2(skillMs < 1500, qPrintable(QString::number(skillMs)));
  QVERIFY(skillForm.collapsedCount() > 0);
  // opening everything is allowed to take longer, but must work
  skillForm.expandAll();
  QCOMPARE(skillForm.collapsedCount(), 0);
}

QTEST_MAIN(FormFeaturesTest)
#include "test_form_features.moc"
