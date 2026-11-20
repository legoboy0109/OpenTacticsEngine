// Scripted UI test of the real main window: open a copy of the demo game, edit through the views, undo/redo, save,
// problems, conflicts, comment warning, Play's command line, remembered state. Dialogs and the player are hooks.
#include <QAction>
#include <QDockWidget>
#include <QListWidget>
#include <QMenu>
#include <QSettings>
#include <QTabWidget>
#include <QTableView>
#include <QTreeWidget>
#include <QUndoView>
#include <QtTest>
#include <memory>

#include "file_list_dock.h"
#include "json_table_model.h"
#include "json_tree_model.h"
#include "table_proxy.h"
#include "main_window.h"
#include "problems_dock.h"
#include "project_session.h"
#include "table_editor.h"
#include "test_prompter.h"
#include "test_support.h"
#include "tree_editor.h"

using namespace ote_native;
using ote::Json;

namespace {
// One scratch area with a settings file and a copy of the demo game; windows are made on request.
struct Env {
  ote_test::Scratch tmp;
  QString game;
  std::unique_ptr<QSettings> settings;
  Env() {
    game = ote_test::copyDemoGame(tmp);
    settings = std::make_unique<QSettings>(tmp.file("settings.ini"), QSettings::IniFormat);
  }
};

struct Win {
  std::unique_ptr<MainWindow> w;
  ote_test::ScriptedPrompter* prompt;
  explicit Win(QSettings* s) {
    w = std::make_unique<MainWindow>(s);
    auto p = std::make_unique<ote_test::ScriptedPrompter>();
    prompt = p.get();
    w->setPrompter(std::move(p));
  }
  // Opens the project and waits until the problems check has finished.
  bool open(const QString& folder) {
    if (!w->openProject(folder)) return false;
    return waitChecked();
  }
  bool waitChecked() {
    ProblemsRunner* r = w->session()->problems();
    QSignalSpy done(r, &ProblemsRunner::finished);
    if (r->isRunning()) return done.wait(60000);
    return true;
  }
  TableEditor* table(const QString& rel) { return qobject_cast<TableEditor*>(w->editorFor(rel)); }
  TreeEditor* tree(const QString& rel) { return qobject_cast<TreeEditor*>(w->editorFor(rel)); }
};

int columnOf(JsonTableModel* m, const char* key) {
  for (int c = 0; c < m->columnCount(); ++c)
    if (m->columnKey(c) == key) return c;
  return -1;
}
}  // namespace

class WindowTest : public QObject {
  Q_OBJECT
 private slots:
  void editUndoRedoSaveAndProblems();
  void aBrokenValueShowsUpAsAProblemNamingTheFile();
  void conflictOffersReloadOrKeepMine();
  void commentWarningComesOnceBeforeTheFirstSave();
  void playBuildsTheCommandLineAndNeverLaunchesAnything();
  void rememberedStateComesBack();
  void openFolderChooserAndRefusals();
  void leavingWithUnsavedWorkAsks();
  void externalChangeIsMarkedInTheFileList();
};

void WindowTest::editUndoRedoSaveAndProblems() {
  Env env;
  Win win(env.settings.get());
  QVERIFY(win.open(env.game));
  QVERIFY2(win.w->problemsDock()->problemCount() == 0, "the demo game is valid");
  QVERIFY(win.w->editorFor("game.json"));  // game.json opens first, as a tree
  QVERIFY(win.tree("game.json"));

  win.w->openFile("data/units.json");
  TableEditor* t = win.table("data/units.json");
  QVERIFY(t);
  QCOMPARE(win.w->tabs()->currentWidget(), t);
  JsonTableModel* m = t->model();
  // The schema makes "stats.hp" a real column: an edit there is a plain cell edit (no JSON dialog any more).
  const int statsCol = columnOf(m, "stats.hp");
  QVERIFY(statsCol >= 0);
  QVERIFY2(columnOf(m, "stats") < 0, "no raw JSON cell where a schema describes the members");
  const int before = m->document()->root()["units"][0]["stats"]["hp"].asInt();

  QVERIFY(t->proxy()->setData(t->proxy()->index(0, statsCol), QString::number(before + 5), Qt::EditRole));
  QCOMPARE(m->document()->root()["units"][0]["stats"]["hp"].asInt(), before + 5);
  QVERIFY(m->document()->isDirty());
  QVERIFY(win.w->tabs()->tabText(win.w->tabs()->indexOf(t)).endsWith("*"));
  QVERIFY(win.w->fileListDock()->textOf("data/units.json").contains("*"));
  QVERIFY(win.w->action("undo")->isEnabled());

  win.w->action("undo")->trigger();
  QCOMPARE(m->document()->root()["units"][0]["stats"]["hp"].asInt(), before);
  QVERIFY(!m->document()->isDirty());
  QVERIFY(!win.w->tabs()->tabText(win.w->tabs()->indexOf(t)).endsWith("*"));
  win.w->action("redo")->trigger();
  QCOMPARE(m->document()->root()["units"][0]["stats"]["hp"].asInt(), before + 5);
  QCOMPARE(win.w->undoView()->stack(), win.w->session()->project()->undoStack());

  QVERIFY(win.w->saveCurrent());
  QVERIFY(win.waitChecked());
  QVERIFY(!m->document()->isDirty());
  Json onDisk;
  std::string err;
  QVERIFY(Json::parse(ote_test::readFile(env.game + "/data/units.json").toStdString(), onDisk, err));
  QCOMPARE(onDisk["units"][0]["stats"]["hp"].asInt(), before + 5);
  QCOMPARE(win.w->problemsDock()->problemCount(), 0);  // still valid after the save
  QVERIFY(win.prompt->errors.isEmpty());
}

void WindowTest::aBrokenValueShowsUpAsAProblemNamingTheFile() {
  Env env;
  Win win(env.settings.get());
  QVERIFY(win.open(env.game));
  win.w->openFile("data/units.json");
  TableEditor* t = win.table("data/units.json");
  JsonTableModel* m = t->model();
  const int col = columnOf(m, "moveType");
  QVERIFY(col >= 0);
  QVERIFY(m->setData(m->index(0, col), "swimming-in-lava", Qt::EditRole));
  win.prompt->commentAnswer = true;
  QVERIFY(win.w->saveCurrent());
  QVERIFY(win.waitChecked());
  QVERIFY2(win.w->problemsDock()->problemCount() >= 1, "the engine check must object to an unknown move type");
  QListWidget* list = win.w->problemsDock()->list();
  QVERIFY2(list->item(0)->text().contains("data/units.json"), qPrintable(list->item(0)->text()));
  QVERIFY(win.w->fileListDock()->textOf("data/units.json").contains("problem"));

  // Activating the problem opens (or selects) the file.
  win.w->openFile("game.json");
  QVERIFY(win.w->tabs()->currentWidget() != t);
  emit list->itemActivated(list->item(0));
  QCOMPARE(win.w->tabs()->currentWidget(), t);

  // Fix it by undo + save: the problem goes away.
  win.w->action("undo")->trigger();
  QVERIFY(win.w->saveCurrent());
  QVERIFY(win.waitChecked());
  QCOMPARE(win.w->problemsDock()->problemCount(), 0);
}

void WindowTest::conflictOffersReloadOrKeepMine() {
  Env env;
  Win win(env.settings.get());
  QVERIFY(win.open(env.game));
  win.w->openFile("data/units.json");
  TableEditor* t = win.table("data/units.json");
  JsonTableModel* m = t->model();
  const int nameCol = columnOf(m, "name");
  QVERIFY(m->setData(m->index(0, nameCol), "Mine", Qt::EditRole));

  // Someone else rewrites the file.
  const QByteArray original = ote_test::readFile(env.game + "/data/units.json");
  QByteArray theirs = original;
  theirs.append("\n");
  ote_test::writeFile(env.game + "/data/units.json", theirs);

  win.prompt->conflictAnswer = Prompter::ConflictChoice::Cancel;
  QVERIFY(!win.w->saveCurrent());
  QCOMPARE(win.prompt->conflictAsked, QStringList{"data/units.json"});
  QCOMPARE(ote_test::readFile(env.game + "/data/units.json"), theirs);  // untouched
  QVERIFY(m->document()->isDirty());

  win.prompt->conflictAnswer = Prompter::ConflictChoice::Reload;
  QVERIFY(!win.w->saveCurrent());  // nothing saved: reloaded instead
  QVERIFY(!m->document()->isDirty());
  QVERIFY(QString::fromStdString(m->document()->root()["units"][0]["name"].asString()) != "Mine");
  QCOMPARE(ote_test::readFile(env.game + "/data/units.json"), theirs);

  QVERIFY(m->setData(m->index(0, nameCol), "Mine again", Qt::EditRole));
  ote_test::writeFile(env.game + "/data/units.json", original);  // changed once more behind our back
  win.prompt->conflictAnswer = Prompter::ConflictChoice::KeepMine;
  QVERIFY(win.w->saveCurrent());
  Json onDisk;
  std::string err;
  Json::parse(ote_test::readFile(env.game + "/data/units.json").toStdString(), onDisk, err);
  QCOMPARE(QString::fromStdString(onDisk["units"][0]["name"].asString()), QString("Mine again"));
  QVERIFY(win.waitChecked());
}

void WindowTest::commentWarningComesOnceBeforeTheFirstSave() {
  Env env;
  Win win(env.settings.get());
  QVERIFY(win.open(env.game));
  QVERIFY(win.w->session()->project()->document("game.json")->hasComments());
  TreeEditor* tree = win.tree("game.json");
  QVERIFY(tree);
  const QModelIndex first = tree->model()->index(0, 0);
  tree->view()->setCurrentIndex(first);
  tree->setKeyPrompt([](const QString&) { return QString("zz_test_key"); });
  QVERIFY(tree->addChild(Json::Type::Number));

  win.prompt->commentAnswer = false;  // the user says no
  QVERIFY(!win.w->saveCurrent());
  QCOMPARE(win.prompt->commentAsked.size(), 1);
  QVERIFY(win.w->session()->project()->document("game.json")->isDirty());
  QVERIFY(ote_test::readFile(env.game + "/game.json").contains("//"));  // not written

  win.prompt->commentAnswer = true;
  QVERIFY(win.w->saveCurrent());
  QCOMPARE(win.prompt->commentAsked.size(), 2);
  QVERIFY(!ote_test::readFile(env.game + "/game.json").contains("\n//"));  // comments gone, as announced
  // The second save of the same file does not ask again.
  QVERIFY(tree->model()->setData(tree->model()->indexForPath({PathStep::member("zz_test_key")}).siblingAtColumn(1), "3", Qt::EditRole));
  QVERIFY(win.w->saveCurrent());
  QCOMPARE(win.prompt->commentAsked.size(), 2);
  QVERIFY(win.waitChecked());
}

void WindowTest::playBuildsTheCommandLineAndNeverLaunchesAnything() {
  Env env;
  Win win(env.settings.get());
  QVERIFY(win.open(env.game));
  QList<PlayCommand> launched;
  win.w->setPlayLauncher([&](const PlayCommand& c) { launched << c; return true; });
  win.w->setApplicationDir(env.tmp.file("bin"));

  QVERIFY(win.w->action("play")->isEnabled());
  win.w->action("play")->trigger();
  QCOMPARE(launched.size(), 1);
  const QString expectedProgram = env.tmp.file("bin/ote-player");
  QCOMPARE(launched[0].program, expectedProgram);
  QCOMPARE(launched[0].args, (QStringList{"--games", env.tmp.path(), "--game", "demo-srpg"}));

  // Unsaved edits: Play asks first; Cancel means no launch.
  win.w->openFile("data/units.json");
  TableEditor* t = win.table("data/units.json");
  t->model()->setData(t->model()->index(0, columnOf(t->model(), "name")), "Changed", Qt::EditRole);
  win.prompt->saveAnswer = Prompter::SaveChoice::Cancel;
  QVERIFY(!win.w->play());
  QCOMPARE(launched.size(), 1);
  QCOMPARE(win.prompt->saveAsked, QStringList{"data/units.json"});
  win.prompt->saveAnswer = Prompter::SaveChoice::Save;
  QVERIFY(win.w->play());
  QCOMPARE(launched.size(), 2);
  QVERIFY(!t->document()->isDirty());  // saved before launching

  // A player path chosen in the settings overrides the default.
  win.w->setPlayerChooser([&] { return env.tmp.file("elsewhere/my-player"); });
  win.w->action("choosePlayer")->trigger();
  QCOMPARE(win.w->playCommand().program, env.tmp.file("elsewhere/my-player"));
  QCOMPARE(env.settings->value("play/playerPath").toString(), env.tmp.file("elsewhere/my-player"));
  QVERIFY(win.waitChecked());
}

void WindowTest::rememberedStateComesBack() {
  Env env;
  {
    Win win(env.settings.get());
    QVERIFY(win.open(env.game));
    win.w->resize(700, 470);  // the offscreen screen is 800x600: Qt would clamp anything bigger on restore
    win.w->problemsDock()->hide();
    win.w->show();
    win.w->close();
  }
  QVERIFY(env.settings->value("recentProjects").toStringList().contains(env.game));
  Win second(env.settings.get());
  second.w->show();
  QCOMPARE(second.w->size(), QSize(700, 470));
  QVERIFY(second.w->problemsDock()->isHidden());
  QVERIFY(second.w->findChild<QMenu*>("recentMenu")->isEnabled());
  QCOMPARE(second.w->findChild<QMenu*>("recentMenu")->actions().first()->text(), env.game);
  QVERIFY(second.w->restoreLastProject());
  QVERIFY(second.waitChecked());
  QCOMPARE(second.w->session()->project()->folder(), env.game);
}

void WindowTest::openFolderChooserAndRefusals() {
  Env env;
  Win win(env.settings.get());
  QString answer = env.game;
  win.w->setFolderChooser([&] { return answer; });
  win.w->action("openFolder")->trigger();
  QVERIFY(win.w->session());
  QVERIFY(win.waitChecked());

  ote_test::writeFile(env.tmp.file("notagame/readme.txt"), "x");
  QVERIFY(!win.w->openProject(env.tmp.file("notagame")));
  QCOMPARE(win.prompt->errors.size(), 1);
  QVERIFY(win.prompt->errors[0].contains("game.json"));
  QCOMPARE(win.w->session()->project()->folder(), env.game);  // still on the old project
  QVERIFY(!env.settings->value("recentProjects").toStringList().contains(env.tmp.file("notagame")));
}

void WindowTest::leavingWithUnsavedWorkAsks() {
  Env env;
  Win win(env.settings.get());
  QVERIFY(win.open(env.game));
  win.w->openFile("data/units.json");
  TableEditor* t = win.table("data/units.json");
  t->model()->setData(t->model()->index(0, columnOf(t->model(), "name")), "Edited", Qt::EditRole);
  win.prompt->saveAnswer = Prompter::SaveChoice::Cancel;
  QVERIFY(!win.w->close());  // refused: still open
  QCOMPARE(win.prompt->saveAsked.size(), 1);
  win.prompt->saveAnswer = Prompter::SaveChoice::Discard;
  QVERIFY(win.w->close());
  Json onDisk;
  std::string err;
  Json::parse(ote_test::readFile(env.game + "/data/units.json").toStdString(), onDisk, err);
  QVERIFY(QString::fromStdString(onDisk["units"][0]["name"].asString()) != "Edited");  // not saved
}

void WindowTest::externalChangeIsMarkedInTheFileList() {
  Env env;
  Win win(env.settings.get());
  QVERIFY(win.open(env.game));
  win.w->openFile("data/units.json");
  QTest::qWait(50);
  QByteArray bytes = ote_test::readFile(env.game + "/data/units.json");
  ote_test::writeFile(env.game + "/data/units.json", bytes + "\n");
  QTRY_VERIFY_WITH_TIMEOUT(win.w->fileListDock()->textOf("data/units.json").contains("changed on disk"), 5000);
  QVERIFY(win.w->tabs()->tabText(win.w->tabs()->currentIndex()).contains("changed on disk"));
}

QTEST_MAIN(WindowTest)
#include "test_window.moc"
