#include "main_window.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QSettings>
#include <QStatusBar>
#include <QTabWidget>
#include <QUndoView>

#include "file_list_dock.h"
#include "id_source.h"
#include "names.h"
#include "problems_dock.h"
#include "project_session.h"
#include "table_editor.h"
#include "table_shape.h"
#include "tree_editor.h"

namespace ote_native {

MainWindow::MainWindow(QSettings* settings, QWidget* parent)
    : QMainWindow(parent), appSettings_(settings), prompter_(std::make_unique<MessageBoxPrompter>(this)),
      appDir_(QCoreApplication::applicationDirPath()) {
  setObjectName("mainWindow");
  setWindowTitle(kWindowTitle);
  tabs_ = new QTabWidget(this);
  tabs_->setObjectName("documentTabs");
  tabs_->setTabsClosable(true);
  tabs_->setMovable(true);
  tabs_->setDocumentMode(true);
  setCentralWidget(tabs_);
  connect(tabs_, &QTabWidget::tabCloseRequested, this, [this](int i) {
    QWidget* w = tabs_->widget(i);
    tabs_->removeTab(i);
    for (auto it = editors_.begin(); it != editors_.end(); ++it)
      if (it.value() == w) { editors_.erase(it); break; }
    for (auto it = treeEditors_.begin(); it != treeEditors_.end(); ++it)
      if (it.value() == w) { treeEditors_.erase(it); break; }
    w->deleteLater();
  });
  connect(tabs_, &QTabWidget::currentChanged, this, [this] { updateUndoActions(); });

  problemsLabel_ = new QLabel(this);
  statusBar()->addPermanentWidget(problemsLabel_);
  buildDocks();
  buildActions();
  problemsLabel_->setText(tr("No project open"));

  resize(1280, 720);
  if (!appSettings_.windowGeometry().isEmpty()) restoreGeometry(appSettings_.windowGeometry());
  if (!appSettings_.windowState().isEmpty()) restoreState(appSettings_.windowState());
  refreshRecentMenu();
}

MainWindow::~MainWindow() {
  // The session (and its worker thread) goes before the widgets that show its results.
  closeProject();
}

void MainWindow::setPrompter(std::unique_ptr<Prompter> p) {
  prompter_ = std::move(p);
  if (session_) session_->setPrompter(prompter_.get());
}

QString MainWindow::statusText() const { return statusBar()->currentMessage(); }

void MainWindow::buildDocks() {
  files_ = new FileListDock(this);
  addDockWidget(Qt::LeftDockWidgetArea, files_);
  connect(files_, &FileListDock::openFileRequested, this, &MainWindow::openFile);

  problems_ = new ProblemsDock(this);
  addDockWidget(Qt::BottomDockWidgetArea, problems_);
  connect(problems_, &ProblemsDock::locateRequested, this, &MainWindow::locateProblem);

  auto* historyDock = new QDockWidget(tr("Undo history"), this);
  historyDock->setObjectName("historyDock");
  undoView_ = new QUndoView(historyDock);
  undoView_->setObjectName("undoView");
  historyDock->setWidget(undoView_);
  addDockWidget(Qt::RightDockWidgetArea, historyDock);
  // Sensible first sizes (a saved layout overrides them): the table gets the room, the lists stay narrow and short.
  resizeDocks({files_, historyDock}, {210, 190}, Qt::Horizontal);
  resizeDocks({problems_}, {120}, Qt::Vertical);
}

QString MainWindow::currentRel() const {
  QWidget* w = tabs_->currentWidget();
  for (auto it = editors_.constBegin(); it != editors_.constEnd(); ++it)
    if (it.value() == w) return it.key();
  for (auto it = treeEditors_.constBegin(); it != treeEditors_.constEnd(); ++it)
    if (it.value() == w) return it.key();
  return {};
}

const SchemaSet& MainWindow::schemas() {
  if (!schemasLoaded_) {
    const QString dir = schemaDir_.isEmpty() ? qEnvironmentVariable("OTE_SCHEMA_DIR") : schemaDir_;
    schemaSet_ = SchemaSet::loadDefault(dir, appDir_);
    schemasLoaded_ = true;
  }
  return schemaSet_;
}

QString MainWindow::tabTitle(const QString& rel) const {
  QString t = rel.section('/', -1);
  if (JsonDocument* d = session_ ? session_->project()->loadedDocument(rel) : nullptr) {
    if (d->isDirty()) t += " *";
    if (d->externallyChanged()) t += tr(" (changed on disk)");
  }
  return t;
}

void MainWindow::refreshTabTitles() {
  for (auto it = editors_.constBegin(); it != editors_.constEnd(); ++it) {
    const int i = tabs_->indexOf(it.value());
    if (i >= 0) { tabs_->setTabText(i, tabTitle(it.key())); tabs_->setTabToolTip(i, it.key()); }
  }
}

void MainWindow::updateFileState(const QString& rel) {
  JsonDocument* d = session_ ? session_->project()->loadedDocument(rel) : nullptr;
  if (!d) return;
  files_->setFileState(rel, d->isDirty(), d->externallyChanged());
  refreshTabTitles();
  if (d->externallyChanged())
    statusBar()->showMessage(tr("%1 was changed on disk. Saving will ask whether to reload it or keep your version.").arg(rel), 10000);
}

void MainWindow::refreshFileList() {
  files_->setFiles(session_ ? session_->project()->jsonFiles() : QStringList());
  if (!session_) return;
  for (const QString& rel : session_->project()->jsonFiles()) updateFileState(rel);
}

void MainWindow::showProblems() {
  const auto& list = session_->problems()->problems();
  problems_->setProblems(list);
  QHash<QString, int> counts;
  for (const auto& p : list) counts[QString::fromStdString(p.file)]++;
  files_->setProblemCounts(counts);
  problemsLabel_->setText(list.empty() ? tr("No problems") : (list.size() == 1 ? tr("1 problem") : tr("%1 problems").arg(list.size())));
}

void MainWindow::closeProject() {
  for (const auto& c : sessionConnections_) disconnect(c);
  sessionConnections_.clear();
  treeEditors_.clear();
  while (tabs_->count() > 0) {
    QWidget* w = tabs_->widget(0);
    tabs_->removeTab(0);
    delete w;
  }
  editors_.clear();
  if (undoView_) undoView_->setStack(nullptr);
  delete ids_;  // the id lists watch documents of the project that is about to go
  ids_ = nullptr;
  delete session_;
  session_ = nullptr;
}

void MainWindow::attachSession(ProjectSession* s) {
  session_ = s;
  Project* p = s->project();
  ids_ = new IdSource(p, this);
  undoView_->setStack(p->undoStack());
  auto link = [this](auto&&... args) { sessionConnections_ << connect(args...); };
  link(p, &Project::fileStateChanged, this, [this](const QString& rel) { updateFileState(rel); });
  link(p, &Project::fileListChanged, this, [this] { refreshFileList(); });
  link(s, &ProjectSession::statusMessage, this, [this](const QString& m) { statusBar()->showMessage(m, 6000); });
  link(s->problems(), &ProblemsRunner::started, this, [this] { problems_->setChecking(); problemsLabel_->setText(tr("Checking...")); });
  link(s->problems(), &ProblemsRunner::finished, this, [this] { showProblems(); });
  QUndoStack* st = p->undoStack();
  link(st, &QUndoStack::canUndoChanged, this, [this] { updateUndoActions(); });
  link(st, &QUndoStack::canRedoChanged, this, [this] { updateUndoActions(); });
  link(st, &QUndoStack::undoTextChanged, this, [this] { updateUndoActions(); });
  link(st, &QUndoStack::redoTextChanged, this, [this] { updateUndoActions(); });
  refreshFileList();
  updateUndoActions();
}

bool MainWindow::openProject(const QString& folder) {
  const QString dir = QDir::cleanPath(QFileInfo(folder).absoluteFilePath());
  if (!QFileInfo(dir + "/game.json").isFile()) {
    prompter_->showError(tr("Not a game folder"), tr("%1 has no game.json, so it is not a game folder.").arg(dir));
    appSettings_.removeRecentProject(dir);
    refreshRecentMenu();
    return false;
  }
  if (session_ && !session_->confirmLeave(tr("You are about to open another folder."))) return false;
  closeProject();
  attachSession(new ProjectSession(dir, prompter_.get(), this));
  appSettings_.addRecentProject(dir);
  appSettings_.setLastProject(dir);
  appSettings_.sync();
  refreshRecentMenu();
  setWindowTitle(QStringLiteral("%1 - %2").arg(QFileInfo(dir).fileName(), kWindowTitle));
  session_->checkProblems();
  if (session_->project()->jsonFiles().contains("game.json")) openFile("game.json");
  return true;
}

bool MainWindow::restoreLastProject() {
  const QString last = appSettings_.lastProject();
  if (last.isEmpty() || !QFileInfo(last + "/game.json").isFile()) return false;
  return openProject(last);
}

void MainWindow::openFile(const QString& rel) {
  if (!session_) return;
  Project* p = session_->project();
  if (!p->jsonFiles().contains(rel)) {
    statusBar()->showMessage(tr("%1 is not a data file this editor can open yet.").arg(rel), 6000);
    return;
  }
  files_->selectFile(rel);
  if (QWidget* existing = editors_.value(rel, nullptr)) {
    tabs_->setCurrentWidget(existing);
    return;
  }
  QString err;
  JsonDocument* doc = p->document(rel, &err);
  if (!doc) {
    prompter_->showError(tr("Cannot open %1").arg(rel), err);
    return;
  }
  QWidget* editor;
  const FileSchema* schema = schemas().forFile(rel);  // none: the plain table / tree of milestone 1
  if (auto arrayPath = findTableArray(doc->root())) {
    auto* t = new TableEditor(p, doc, *arrayPath, tabs_, schema, ids_);
    connect(t, &TableEditor::messageShown, this, [this](const QString& m) { statusBar()->showMessage(m, 8000); });
    connect(t, &TableEditor::showInTreeRequested, this, [this, rel](const JsonPath& path) { openTreeView(rel, path); });
    if (schema && schema->rows) {  // the table | table + form | form choice is remembered per kind of file
      const QString kind = QString::fromStdString(schema->name);
      t->setViewMode(appSettings_.tableViewMode(kind));
      connect(t, &TableEditor::viewModeChanged, this, [this, kind](int mode) {
        appSettings_.setTableViewMode(kind, mode);
        appSettings_.sync();
      });
    }
    editor = t;
  } else {
    auto* t = new TreeEditor(p, doc, tabs_, schema, ids_);
    t->setAppSettings(&appSettings_);
    connect(t, &TreeEditor::messageShown, this, [this](const QString& m) { statusBar()->showMessage(m, 8000); });
    editor = t;
  }
  editors_.insert(rel, editor);
  tabs_->setCurrentIndex(tabs_->addTab(editor, tabTitle(rel)));
  refreshTabTitles();
}

void MainWindow::locateProblem(const QString& rel, const QString& message) {
  if (!session_ || !session_->project()->jsonFiles().contains(rel)) {
    statusBar()->showMessage(tr("%1 is not a data file this editor can open yet.").arg(rel), 6000);
    return;
  }
  openFile(rel);
  QWidget* e = editors_.value(rel, nullptr);
  if (auto* t = qobject_cast<TableEditor*>(e)) t->locateProblem(message);
  else if (auto* tr2 = qobject_cast<TreeEditor*>(e)) tr2->locateProblem(message);
}

void MainWindow::openTreeView(const QString& rel, const JsonPath& path) {
  if (!session_) return;
  QWidget* w = treeEditors_.value(rel, nullptr);
  if (!w) {
    JsonDocument* doc = session_->project()->document(rel);
    if (!doc) return;
    auto* t = new TreeEditor(session_->project(), doc, tabs_);  // no schema: the plain tree of every key
    connect(t, &TreeEditor::messageShown, this, [this](const QString& m) { statusBar()->showMessage(m, 8000); });
    treeEditors_.insert(rel, t);
    tabs_->addTab(t, tr("%1 (tree)").arg(rel.section('/', -1)));
    w = t;
  }
  tabs_->setCurrentWidget(w);
  static_cast<TreeEditor*>(w)->selectPath(path);
}

void MainWindow::closeEvent(QCloseEvent* ev) {
  if (session_ && !session_->confirmLeave(tr("You are about to close the editor."))) {
    ev->ignore();
    return;
  }
  appSettings_.saveWindow(saveGeometry(), saveState());
  appSettings_.sync();
  QMainWindow::closeEvent(ev);
}

}  // namespace ote_native
