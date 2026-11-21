// A scripted run over a copy of games/demo-srpg: every data file is opened, every row (the first 50) selected once and its form
// opened completely. Untouched valid data must show no warning mark, files the schemas describe must have an empty "Other keys"
// area (maps keep the raw parts listed in the conformance tables), and saving must change nothing it was not asked to change.
#include <QCheckBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSettings>
#include <QSignalSpy>
#include <QToolButton>
#include <QtTest>
#include <memory>

#include "form_context.h"
#include "form_view.h"
#include "json_table_model.h"
#include "main_window.h"
#include "project_session.h"
#include "table_editor.h"
#include "test_prompter.h"
#include "test_support.h"
#include "tree_editor.h"

using namespace ote_native;
using ote::Json;

namespace {

struct Win {
  ote_test::Scratch tmp;
  std::unique_ptr<QSettings> settings;
  std::unique_ptr<MainWindow> w;
  QString folder;
  Win() {
    settings = std::make_unique<QSettings>(tmp.file("s.ini"), QSettings::IniFormat);
    w = std::make_unique<MainWindow>(settings.get());
    w->setPrompter(std::make_unique<ote_test::ScriptedPrompter>());
    w->setSchemaDir(QStringLiteral(OTE_SOURCE_DIR "/editor/schema"));
    w->resize(1280, 720);
    w->show();
  }
  bool open(const QString& game = "demo-srpg") {
    folder = tmp.file(game);
    ote_test::copyTree(QStringLiteral(OTE_SOURCE_DIR "/games/") + game, folder);
    if (!w->openProject(folder)) return false;
    ProblemsRunner* r = w->session()->problems();
    QSignalSpy done(r, &ProblemsRunner::finished);
    return !r->isRunning() || done.wait(60000);
  }
};

// Everything a person would read as "something is off" in a form: a visible note under a field, a refused-edit message, an id the
// project does not have, a field that does not apply.
QStringList warningsOf(FormView* f) {
  QStringList out;
  for (QLabel* l : f->findChildren<QLabel*>())
    if (l->isVisibleTo(f) && !l->text().isEmpty() &&
        (l->objectName() == "fieldNote" || l->objectName() == "notApplicable" || l->objectName() == "formMessage" || l->text().contains("(unknown id)")))
      out << l->text();
  return out;
}

// The keys listed in the "Other keys" boxes of a form.
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

// Files whose schema deliberately leaves some keys raw: a map's tile grid (a map painter is a later milestone).
bool rawByDesign(const QString& rel, const QString& key) {
  return rel.startsWith("maps/") && QStringList{"tiles", "tileLayer", "heights"}.contains(key);
}

}  // namespace

class FormDemoTest : public QObject {
  Q_OBJECT
 private slots:
  void everyDemoFileOpensCleanAndEveryRowShowsNoWarning();
  void savingWithoutEditsWritesNothingAndAnEditChangesOnlyItself();
  void theCheckSeesAWrongValueAndAnUnknownKey();
  void savingUntouchedCopiesOfEveryDemoRewritesNothing();
};

void FormDemoTest::everyDemoFileOpensCleanAndEveryRowShowsNoWarning() {
  int forms = 0, rows = 0, events = 0, commands = 0;
  QStringList problems;
  // The three demo games, as copies: every file opens, every table row and every event of every map shows its form completely.
  for (const QString game : {"demo-srpg", "demo-economy", "demo-hex"}) {
    Win s;
    QVERIFY2(s.open(game), qPrintable(game));
    Project* p = s.w->session()->project();
    for (const QString& rel : p->jsonFiles()) {
      s.w->openFile(rel);
      QWidget* e = s.w->editorFor(rel);
      QVERIFY2(e, qPrintable(rel));
      auto check = [&](FormView* f, const QString& where) {
        f->expandAll();
        f->flush();
        QCoreApplication::processEvents();
        ++forms;
        for (const QString& w : warningsOf(f)) problems << game + " " + where + ": warning shown: " + w.left(100);
        for (const QString& k : otherKeysOf(f))
          if (!rawByDesign(rel, k)) problems << game + " " + where + ": key under Other keys: " + k;
      };
      if (auto* t = qobject_cast<TableEditor*>(e)) {
        if (!t->form()) continue;  // no schema: the plain table
        const int n = std::min(t->model()->rowCount(), 50);
        for (int r = 0; r < n; ++r) {
          t->selectRow(r);
          QTest::qWait(0);
          ++rows;
          check(t->form(), rel + " row " + QString::number(r));
        }
      } else if (auto* tr = qobject_cast<TreeEditor*>(e)) {
        if (tr->form()) {
          check(tr->form(), rel);
          // Every command of every event is a form of its own: a command that no variant describes would stand in as raw JSON.
          const Json* ev = tr->formContext()->at({PathStep::member("events")});
          for (size_t i = 0; ev && i < ev->size(); ++i) {
            ++events;
            for (size_t c = 0; c < (*ev)[i]["do"].size(); ++c) {
              ++commands;
              JsonPath cp = {PathStep::member("events"), PathStep::at(i), PathStep::member("do"), PathStep::at(c)};
              if (!tr->form()->hasAnchor(cp))
                problems << game + " " + rel + ": command " + QString::number(c + 1) + " of event " + QString::number(i + 1) + " is not shown as a form";
            }
          }
        }
      }
    }
  }
  qInfo("demo run: %d forms checked, %d table rows selected, %d events with %d commands", forms, rows, events, commands);
  QVERIFY(forms > 40);
  QVERIFY(events >= 8 && commands >= 20);
  QVERIFY2(problems.isEmpty(), qPrintable(problems.join("\n")));
}

void FormDemoTest::savingWithoutEditsWritesNothingAndAnEditChangesOnlyItself() {
  Win s;
  QVERIFY(s.open());
  Project* p = s.w->session()->project();
  // Touch every file with an edit and undo it: the files are clean again and Save all must not rewrite them.
  QMap<QString, QByteArray> before;
  for (const QString& rel : p->jsonFiles()) before[rel] = ote_test::readFile(s.folder + "/" + rel);
  s.w->openFile("data/units.json");
  auto* t = qobject_cast<TableEditor*>(s.w->editorFor("data/units.json"));
  QVERIFY(t && t->form());
  t->selectRow(0);
  JsonPath name = t->model()->arrayPath();
  name.push_back(PathStep::at(0));
  name.push_back(PathStep::member("name"));
  const QString oldName = QString::fromStdString((*t->formContext()->at(name)).asString());
  t->formContext()->set(name, Json::makeString("Changed"));
  p->undoStack()->undo();
  QVERIFY(s.w->saveAll());
  for (auto it = before.cbegin(); it != before.cend(); ++it)
    QVERIFY2(ote_test::readFile(s.folder + "/" + it.key()) == it.value(), qPrintable(it.key() + " changed although nothing was edited"));

  // A real edit rewrites only its own file, and the file differs from the original only by the edit, dropped comments and
  // number formatting.
  t->formContext()->set(name, Json::makeString("Changed"));
  QVERIFY(s.w->saveAll());
  for (auto it = before.cbegin(); it != before.cend(); ++it) {
    const QByteArray now = ote_test::readFile(s.folder + "/" + it.key());
    if (it.key() == "data/units.json") continue;
    QVERIFY2(now == it.value(), qPrintable(it.key() + " was rewritten by an edit of units.json"));
  }
  const QByteArray saved = ote_test::readFile(s.folder + "/data/units.json");
  Json a, b;
  std::string err;
  QVERIFY(Json::parse(before["data/units.json"].toStdString(), a, err));  // the reader accepts the original's comments
  QVERIFY(Json::parse(saved.toStdString(), b, err));
  // put the old name back in the saved copy: the two trees must then be identical
  auto rowOf = [](Json& j) -> Json& { return j.mutableMembers()[0].second.mutableItems()[0]; };
  for (auto& m : rowOf(b).mutableMembers())
    if (m.first == "name") m.second = Json::makeString(oldName.toStdString());
  QCOMPARE(QString::fromStdString(b.dump(0)), QString::fromStdString(a.dump(0)));
  QVERIFY2(!saved.contains("//"), "the saved file keeps no comments (known: the JSON tree does not hold them)");
}

// Open every file of each demo copy (forms built, events expanded), change nothing, Save all: not one byte moves.
void FormDemoTest::savingUntouchedCopiesOfEveryDemoRewritesNothing() {
  for (const QString game : {"demo-srpg", "demo-economy", "demo-hex"}) {
    Win s;
    QVERIFY2(s.open(game), qPrintable(game));
    Project* p = s.w->session()->project();
    QMap<QString, QByteArray> before;
    for (const QString& rel : p->jsonFiles()) {
      before[rel] = ote_test::readFile(s.folder + "/" + rel);
      s.w->openFile(rel);
      if (auto* tr = qobject_cast<TreeEditor*>(s.w->editorFor(rel)))
        if (tr->form()) tr->form()->expandAll();
    }
    QVERIFY(s.w->saveAll());
    for (auto it = before.cbegin(); it != before.cend(); ++it)
      QVERIFY2(ote_test::readFile(s.folder + "/" + it.key()) == it.value(), qPrintable(game + "/" + it.key() + " changed although nothing was edited"));
  }
}

// The two checks above are only worth something when they can fail: break a row on purpose and they must notice.
void FormDemoTest::theCheckSeesAWrongValueAndAnUnknownKey() {
  Win s;
  QVERIFY(s.open());
  s.w->openFile("data/weapons.json");
  auto* t = qobject_cast<TableEditor*>(s.w->editorFor("data/weapons.json"));
  QVERIFY(t && t->form());
  t->selectRow(0);
  t->form()->expandAll();
  QVERIFY(warningsOf(t->form()).isEmpty());
  JsonPath row = t->model()->arrayPath();
  row.push_back(PathStep::at(0));
  JsonPath counter = row, extra = row;
  counter.push_back(PathStep::member("counter"));
  extra.push_back(PathStep::member("futureKey"));
  t->formContext()->set(counter, Json::makeString("yes"));
  t->formContext()->set(extra, Json::makeNumber(1));
  t->form()->flush();
  t->form()->expandAll();
  QVERIFY(!warningsOf(t->form()).isEmpty());
  QVERIFY(otherKeysOf(t->form()).contains("futureKey"));
}

QTEST_MAIN(FormDemoTest)
#include "test_form_demo.moc"
