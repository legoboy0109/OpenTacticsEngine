// The editor window: project file list, tabbed documents (table or tree per file), problems, undo history, and the
// File / Edit / Run actions. Layout and recent projects are remembered through AppSettings.
//
// Everything that would block a test (message boxes, the folder dialog, starting the player) goes through a hook, so the
// scripted UI tests drive the real window without dialogs and without ever launching anything.
#pragma once
#include <QHash>
#include <QMainWindow>
#include <functional>
#include <memory>

#include "app_settings.h"
#include "play_command.h"
#include "prompter.h"

class QAction;
class QLabel;
class QMenu;
class QSettings;
class QTabWidget;
class QUndoView;

namespace ote_native {

class FileListDock;
class ProblemsDock;
class ProjectSession;

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QSettings* settings, QWidget* parent = nullptr);
  ~MainWindow() override;

  // Opens a game folder (needs a game.json). Asks about unsaved work in the current project first. False when refused.
  bool openProject(const QString& folder);
  // Reopens the project of the last run, if it still exists. Used by the program's start-up.
  bool restoreLastProject();
  void openFile(const QString& relPath);
  bool saveCurrent();
  bool saveAll();
  // Runs ote-player on the open project as a separate process (after offering to save).
  bool play();
  PlayCommand playCommand() const;

  // --- hooks (tests replace them) ---
  void setPrompter(std::unique_ptr<Prompter> p);
  void setFolderChooser(std::function<QString()> f) { folderChooser_ = std::move(f); }
  void setPlayerChooser(std::function<QString()> f) { playerChooser_ = std::move(f); }
  void setPlayLauncher(std::function<bool(const PlayCommand&)> f) { launcher_ = std::move(f); }
  void setApplicationDir(const QString& dir) { appDir_ = dir; }

  // --- what tests look at ---
  ProjectSession* session() const { return session_; }
  QTabWidget* tabs() const { return tabs_; }
  ProblemsDock* problemsDock() const { return problems_; }
  FileListDock* fileListDock() const { return files_; }
  QUndoView* undoView() const { return undoView_; }
  QWidget* editorFor(const QString& rel) const { return editors_.value(rel, nullptr); }
  QAction* action(const QString& name) const { return actions_.value(name, nullptr); }
  QString statusText() const;
  AppSettings& appSettings() { return appSettings_; }

 protected:
  void closeEvent(QCloseEvent* ev) override;

 private:
  void buildActions();
  void buildDocks();
  void closeProject();
  void attachSession(ProjectSession* s);
  void updateFileState(const QString& rel);
  void updateUndoActions();
  void refreshRecentMenu();
  void refreshFileList();
  void showProblems();
  void chooseFolder();
  void choosePlayer();
  void reloadCurrent();
  QString currentRel() const;
  QString tabTitle(const QString& rel) const;
  void refreshTabTitles();
  QAction* addAction(const QString& name, const QString& text, const QKeySequence& key, std::function<void()> fn, QMenu* menu,
                     bool toolbar = false);

  AppSettings appSettings_;
  std::unique_ptr<Prompter> prompter_;
  ProjectSession* session_ = nullptr;
  QTabWidget* tabs_;
  FileListDock* files_ = nullptr;
  ProblemsDock* problems_ = nullptr;
  QUndoView* undoView_ = nullptr;
  QLabel* problemsLabel_;
  QMenu* recentMenu_ = nullptr;
  QHash<QString, QAction*> actions_;
  QHash<QString, QWidget*> editors_;
  QList<QMetaObject::Connection> sessionConnections_;
  std::function<QString()> folderChooser_, playerChooser_;
  std::function<bool(const PlayCommand&)> launcher_;
  QString appDir_;
};

}  // namespace ote_native
