// Scripted run of the real main window on a temp copy of a fixture, using several of the new mechanics through the
// forms only: add a resource, enable action points on the Rules page, add a damage type, give a weapon that damage type
// through its combo box, save, problems empty; undo everything -> every file is as it was loaded. Also: problems that
// name a row and key are placed, and "Show in tree view" opens the tree of a table file. No dialog, no player, no sound.
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QTabWidget>
#include <QUndoStack>
#include <QtTest>
#include <memory>

#include <QTreeView>

#include "form_fields.h"
#include "json_table_model.h"
#include "json_commands.h"
#include "form_view.h"
#include "main_window.h"
#include "problems_dock.h"
#include "project_session.h"
#include "table_editor.h"
#include "test_prompter.h"
#include "test_support.h"
#include "tree_editor.h"

using namespace ote_native;

namespace {

struct Win {
  ote_test::Scratch tmp;
  std::unique_ptr<QSettings> settings;
  std::unique_ptr<MainWindow> w;
  ote_test::ScriptedPrompter* prompt = nullptr;
  QString folder;

  Win() {
    settings = std::make_unique<QSettings>(tmp.file("s.ini"), QSettings::IniFormat);
    w = std::make_unique<MainWindow>(settings.get());
    auto p = std::make_unique<ote_test::ScriptedPrompter>();
    prompt = p.get();
    w->setPrompter(std::move(p));
    w->setSchemaDir(QStringLiteral(OTE_SOURCE_DIR "/editor/schema"));
    w->resize(1280, 720);
    w->show();
  }
  bool openFixture(const QString& name) {
    folder = ote_test::copyFixture(tmp, name);
    if (!w->openProject(folder)) return false;
    return waitChecked();
  }
  bool waitChecked() {
    ProblemsRunner* r = w->session()->problems();
    QSignalSpy done(r, &ProblemsRunner::finished);
    return !r->isRunning() || done.wait(60000);
  }
  void settle(FormView* f) { f->flush(); QCoreApplication::processEvents(); f->flush(); }
};

JsonPath path(std::initializer_list<const char*> keys) {
  JsonPath p;
  for (const char* k : keys) p.push_back(PathStep::member(k));
  return p;
}
JsonPath index(JsonPath p, size_t i) {
  p.push_back(PathStep::at(i));
  return p;
}
JsonPath key(JsonPath p, const char* k) {
  p.push_back(PathStep::member(k));
  return p;
}

}  // namespace

class FormWindowTest : public QObject {
  Q_OBJECT
 private slots:
  void severalMechanicsThroughTheFormsThenUndoEverything();
  void problemsDoubleClickPlacesTheCursor();
  void showInTreeOpensTheTreeOfATableFile();
  void withoutASchemaNothingChanges();
};

void FormWindowTest::severalMechanicsThroughTheFormsThenUndoEverything() {
  Win win;
  QVERIFY(win.openFixture("factions"));
  QCOMPARE(win.w->problemsDock()->problemCount(), 0);
  Project* project = win.w->session()->project();
  JsonDocument* game = project->document("game.json");
  JsonDocument* weapons = project->document("data/weapons.json");
  const QByteArray gameOriginal = game->serialize(), weaponsOriginal = weapons->serialize();

  // --- Rules page: a new resource "ap", then the action-point block with that resource ---------------------------
  auto* tree = qobject_cast<TreeEditor*>(win.w->editorFor("game.json"));
  QVERIFY(tree && tree->form());
  tree->showFormPage();
  FormView* form = tree->form();
  win.settle(form);
  auto addButtonOf = [&](const char* listPath) -> QPushButton* {
    for (QWidget* l : form->findChildren<QWidget*>("listField"))
      if (l->property("path").toString() == listPath) return l->findChild<QPushButton*>("addItem");
    return nullptr;
  };
  QPushButton* addResource = addButtonOf("resources");
  QVERIFY(addResource);
  addResource->click();
  win.settle(form);
  auto* newId = qobject_cast<ScalarField*>(form->fieldAt(key(index(path({"resources"}), 1), "id")));
  QVERIFY(newId);
  newId->edit()->setText("ap");
  emit newId->edit()->editingFinished();

  QCheckBox* enableAp = nullptr;
  for (auto* c : form->findChildren<QCheckBox*>("blockEnable")) if (c->text() == "actionPoints") enableAp = c;
  QVERIFY(enableAp && !enableAp->isChecked());
  enableAp->click();
  win.settle(form);
  QVERIFY(game->root()["rules"].has("actionPoints"));
  auto* resource = qobject_cast<ChoiceField*>(form->fieldAt(path({"rules", "actionPoints", "resource"})));
  QVERIFY(resource);
  QVERIFY(resource->combo()->findData("ap") >= 0);  // the combo already lists the id that was just typed
  const int apIdx = resource->combo()->findData("ap");
  resource->combo()->setCurrentIndex(apIdx);
  emit resource->combo()->activated(apIdx);
  QCOMPARE(QString::fromStdString(game->root()["rules"]["actionPoints"]["resource"].asString()), QString("ap"));

  // --- a damage type -------------------------------------------------------------------------------------------
  QPushButton* addType = addButtonOf("damageTypes");
  QVERIFY(addType);
  addType->click();
  win.settle(form);
  auto* typeId = qobject_cast<ScalarField*>(form->fieldAt(key(index(path({"damageTypes"}), 0), "id")));
  QVERIFY(typeId);
  typeId->edit()->setText("arc");
  emit typeId->edit()->editingFinished();

  // --- a weapon gets that damage type through its combo box ------------------------------------------------------
  win.w->openFile("data/weapons.json");
  auto* table = qobject_cast<TableEditor*>(win.w->editorFor("data/weapons.json"));
  QVERIFY(table && table->form());
  table->selectRow(0);
  win.settle(table->form());
  auto* damageType = qobject_cast<ChoiceField*>(table->form()->fieldAt(key(index(path({"weapons"}), 0), "damageType")));
  QVERIFY(damageType);
  const int arc = damageType->combo()->findData("arc");
  QVERIFY2(arc >= 0, "the new damage type is offered at once");
  damageType->combo()->setCurrentIndex(arc);
  emit damageType->combo()->activated(arc);
  QCOMPARE(QString::fromStdString(weapons->root()["weapons"][size_t(0)]["damageType"].asString()), QString("arc"));

  // --- save, problems empty ---------------------------------------------------------------------------------
  QVERIFY(win.w->saveAll());
  QVERIFY(win.waitChecked());
  QVERIFY2(win.w->problemsDock()->problemCount() == 0, "the engine accepts what the forms wrote");
  QVERIFY(win.prompt->errors.isEmpty());
  {
    ote::Json onDisk;
    std::string err;
    QVERIFY(ote::Json::parse(ote_test::readFile(win.folder + "/game.json").toStdString(), onDisk, err));
    QVERIFY(onDisk["rules"].has("actionPoints"));
    QCOMPARE(QString::fromStdString(onDisk["damageTypes"][size_t(0)]["id"].asString()), QString("arc"));
    QCOMPARE(onDisk["resources"].size(), size_t(2));
  }

  // --- undo everything: each document is exactly as loaded --------------------------------------------------------
  project->undoStack()->setIndex(0);
  win.settle(form);
  QCOMPARE(game->serialize(), gameOriginal);
  QCOMPARE(weapons->serialize(), weaponsOriginal);
  QVERIFY(game->root()["rules"].has("actionPoints") == false);
  // The form followed: no "ap" resource left, the block is off again.
  QVERIFY(!form->fieldAt(key(index(path({"resources"}), 1), "id")));
}

void FormWindowTest::problemsDoubleClickPlacesTheCursor() {
  Win win;
  QVERIFY(win.openFixture("damage-types"));
  Project* project = win.w->session()->project();
  JsonDocument* weapons = project->document("data/weapons.json");
  // Break row 2's damage type on disk, so the engine's own message names the weapon and the key.
  const QString id = QString::fromStdString(weapons->root()["weapons"][size_t(2)]["id"].asString());
  {
    ote::Json w = weapons->root()["weapons"][size_t(2)];
    w.set("damageType", ote::Json::makeString("nonexistent"));
    project->push(weapons, new SetValueCommand(weapons, index(path({"weapons"}), 2), w));
  }
  QVERIFY(win.w->saveAll());
  QVERIFY(win.waitChecked());
  QVERIFY(win.w->problemsDock()->problemCount() >= 1);
  win.w->openFile("data/units.json");  // another tab is in front
  QListWidget* list = win.w->problemsDock()->list();
  int found = -1;
  for (int i = 0; i < list->count(); ++i) if (list->item(i)->text().contains(id)) found = i;
  QVERIFY2(found >= 0, qPrintable(list->item(0)->text()));
  emit list->itemActivated(list->item(found));  // what a double-click does
  auto* table = qobject_cast<TableEditor*>(win.w->editorFor("data/weapons.json"));
  QVERIFY(table);
  QCOMPARE(win.w->tabs()->currentWidget(), table);
  QCOMPARE(table->currentSourceRow(), 2);  // the row of the weapon the message names
  QVERIFY(table->form()->hasTarget());
  FieldBase* f = table->form()->fieldAt(key(index(path({"weapons"}), 2), "damageType"));
  QVERIFY(f);
  QVERIFY(table->form()->target().back().isIndex);  // the form shows a row of the weapons list
}

void FormWindowTest::showInTreeOpensTheTreeOfATableFile() {
  Win win;
  QVERIFY(win.openFixture("damage-types"));
  win.w->openFile("data/units.json");
  auto* table = qobject_cast<TableEditor*>(win.w->editorFor("data/units.json"));
  QVERIFY(table);
  QVERIFY(!win.w->treeEditorFor("data/units.json"));
  emit table->showInTreeRequested(index(path({"units"}), 1));
  auto* tree = qobject_cast<TreeEditor*>(win.w->treeEditorFor("data/units.json"));
  QVERIFY(tree);
  QCOMPARE(win.w->tabs()->currentWidget(), tree);
  QVERIFY(tree->view()->currentIndex().isValid());
}

void FormWindowTest::withoutASchemaNothingChanges() {
  Win win;
  QDir().mkpath(win.tmp.file("no-schemas"));
  win.w->setSchemaDir(win.tmp.file("no-schemas"));  // an existing, empty folder: no schema for any file
  QVERIFY(win.openFixture("damage-types"));
  win.w->openFile("data/weapons.json");
  auto* table = qobject_cast<TableEditor*>(win.w->editorFor("data/weapons.json"));
  QVERIFY(table);
  QVERIFY(!table->form());  // no schema: the plain table of milestone 1
  auto* tree = qobject_cast<TreeEditor*>(win.w->editorFor("game.json"));
  QVERIFY(tree && !tree->form() && !tree->pages());
  QStringList cols;
  for (int c = 0; c < table->model()->columnCount(); ++c) cols << table->model()->columnKey(c);
  QVERIFY(cols.contains("props"));  // nested values are raw JSON cells again
}

QTEST_MAIN(FormWindowTest)
#include "test_form_window.moc"
