// MainWindow, part 2: menus, toolbar and what the actions do.
#include <QAction>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QUndoStack>

#include "main_window.h"
#include "project_session.h"
#include "problems_dock.h"
#include "file_list_dock.h"

namespace ote_native {

QAction* MainWindow::addAction(const QString& name, const QString& text, const QKeySequence& key, std::function<void()> fn, QMenu* menu,
                               bool toolbar) {
  auto* a = new QAction(text, this);
  a->setObjectName(name);
  if (!key.isEmpty()) a->setShortcut(key);
  connect(a, &QAction::triggered, this, [fn = std::move(fn)] { fn(); });
  menu->addAction(a);
  if (toolbar) findChild<QToolBar*>("mainToolbar")->addAction(a);
  actions_.insert(name, a);
  return a;
}

void MainWindow::buildActions() {
  auto* bar = addToolBar(tr("Main"));
  bar->setObjectName("mainToolbar");
  bar->setMovable(false);

  QMenu* file = menuBar()->addMenu(tr("&File"));
  addAction("openFolder", tr("&Open folder..."), QKeySequence::Open, [this] { chooseFolder(); }, file, true);
  recentMenu_ = file->addMenu(tr("Open &recent"));
  recentMenu_->setObjectName("recentMenu");
  file->addSeparator();
  addAction("save", tr("&Save"), QKeySequence::Save, [this] { saveCurrent(); }, file, true);
  addAction("saveAll", tr("Save &all"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), [this] { saveAll(); }, file, true);
  addAction("reload", tr("Re&load file from disk"), QKeySequence(), [this] { reloadCurrent(); }, file);
  file->addSeparator();
  addAction("quit", tr("&Quit"), QKeySequence::Quit, [this] { close(); }, file);

  QMenu* edit = menuBar()->addMenu(tr("&Edit"));
  addAction("undo", tr("&Undo"), QKeySequence::Undo,
            [this] { if (session_) session_->project()->undoStack()->undo(); }, edit, true);
  addAction("redo", tr("&Redo"), QKeySequence::Redo,
            [this] { if (session_) session_->project()->undoStack()->redo(); }, edit, true);
  edit->addSeparator();
  addAction("choosePlayer", tr("Player program..."), QKeySequence(), [this] { choosePlayer(); }, edit);

  QMenu* run = menuBar()->addMenu(tr("&Run"));
  addAction("play", tr("&Play"), QKeySequence(Qt::Key_F5), [this] { play(); }, run, true);

  QMenu* view = menuBar()->addMenu(tr("&View"));
  for (QDockWidget* d : findChildren<QDockWidget*>()) view->addAction(d->toggleViewAction());
  updateUndoActions();
}

void MainWindow::updateUndoActions() {
  QUndoStack* st = session_ ? session_->project()->undoStack() : nullptr;
  QAction* u = action("undo");
  QAction* r = action("redo");
  if (!u || !r) return;
  u->setEnabled(st && st->canUndo());
  r->setEnabled(st && st->canRedo());
  u->setText(st && st->canUndo() ? tr("&Undo %1").arg(st->undoText()) : tr("&Undo"));
  r->setText(st && st->canRedo() ? tr("&Redo %1").arg(st->redoText()) : tr("&Redo"));
  const bool open = session_ != nullptr;
  for (const char* n : {"save", "saveAll", "play", "reload"})
    if (QAction* a = action(n)) a->setEnabled(open);
}

void MainWindow::refreshRecentMenu() {
  if (!recentMenu_) return;
  recentMenu_->clear();
  const QStringList recent = appSettings_.recentProjects();
  for (const QString& dir : recent) {
    QAction* a = recentMenu_->addAction(dir);
    connect(a, &QAction::triggered, this, [this, dir] { openProject(dir); });
  }
  recentMenu_->setEnabled(!recent.isEmpty());
}

void MainWindow::chooseFolder() {
  QString dir;
  if (folderChooser_) {
    dir = folderChooser_();
  } else {
    // Qt's own dialog, not the desktop portal's: the portal dialog proved unreliable on the owner's machine.
    QString start = appSettings_.lastProject();
    start = start.isEmpty() ? QDir::homePath() : QFileInfo(start).absolutePath();
    dir = QFileDialog::getExistingDirectory(this, tr("Open game folder"), start,
                                            QFileDialog::ShowDirsOnly | QFileDialog::DontUseNativeDialog);
  }
  if (!dir.isEmpty()) openProject(dir);
}

void MainWindow::choosePlayer() {
  QString path;
  if (playerChooser_) {
    path = playerChooser_();
  } else {
    path = QFileDialog::getOpenFileName(this, tr("Choose the player program (ote-player)"), appDir_, QString(),
                                        nullptr, QFileDialog::DontUseNativeDialog);
  }
  if (path.isEmpty()) return;
  appSettings_.setPlayerPathOverride(path);
  appSettings_.sync();
  statusBar()->showMessage(tr("Play will use %1").arg(path), 6000);
}

bool MainWindow::saveCurrent() {
  const QString rel = currentRel();
  return session_ && !rel.isEmpty() ? session_->save(rel) : false;
}

bool MainWindow::saveAll() { return session_ ? session_->saveAll() : false; }

void MainWindow::reloadCurrent() {
  const QString rel = currentRel();
  if (!session_ || rel.isEmpty()) return;
  JsonDocument* d = session_->project()->loadedDocument(rel);
  if (d && d->isDirty()) {
    switch (prompter_->askSaveChanges({rel}, tr("Reloading drops your unsaved changes to this file."))) {
      case Prompter::SaveChoice::Cancel: return;
      case Prompter::SaveChoice::Save: if (!session_->save(rel)) return; break;
      case Prompter::SaveChoice::Discard: break;
    }
  }
  QString err;
  if (!session_->project()->reload(rel, &err)) prompter_->showError(tr("Reload failed"), err);
  else statusBar()->showMessage(tr("Reloaded %1 from disk").arg(rel), 6000);
}

PlayCommand MainWindow::playCommand() const {
  return makePlayCommand(session_ ? session_->project()->folder() : QString(), appSettings_.playerPathOverride(), appDir_);
}

bool MainWindow::play() {
  if (!session_) return false;
  // The player reads the files on disk, so unsaved edits would not be in what it shows.
  if (!session_->confirmLeave(tr("The player starts from the saved files."))) return false;
  const PlayCommand cmd = playCommand();
  if (launcher_) {
    return launcher_(cmd);
  }
  if (!QFileInfo(cmd.program).isExecutable()) {
    prompter_->showError(tr("Player not found"),
                         tr("%1 does not exist. Build ote-player next to the editor, or choose it under Edit > Player program.").arg(cmd.program));
    return false;
  }
  const bool ok = startPlayer(cmd);
  if (!ok) prompter_->showError(tr("Player did not start"), tr("Could not run %1").arg(cmd.program));
  else statusBar()->showMessage(tr("Started the player for %1").arg(QFileInfo(session_->project()->folder()).fileName()), 6000);
  return ok;
}

}  // namespace ote_native
