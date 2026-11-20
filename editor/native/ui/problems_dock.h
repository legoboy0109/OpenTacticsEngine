// The Problems dock: what the engine's project check found in the saved files. Activating (double-click / Enter) a
// problem that names a file asks the window to open that file.
#pragma once
#include <QDockWidget>
#include <vector>

#include "common.h"

class QListWidget;

namespace ote_native {

class ProblemsDock : public QDockWidget {
  Q_OBJECT
 public:
  explicit ProblemsDock(QWidget* parent = nullptr);

  void setChecking();  // shown while a check runs
  void setProblems(const std::vector<ote_editor::Problem>& problems);
  int problemCount() const { return count_; }
  QListWidget* list() const { return list_; }

 signals:
  void openFileRequested(const QString& relPath);
  // Same activation, with the loader's message so the editor can place the cursor on the row / key it names.
  void locateRequested(const QString& relPath, const QString& message);

 private:
  QListWidget* list_;
  int count_ = 0;
};

}  // namespace ote_native
