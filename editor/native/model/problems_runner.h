// Runs the engine's own project check (ote_editor_core's validateProject: loadGame, every map, formulas, a battle start,
// the sound check) on the SAVED files of a folder, on a worker thread so the window never waits for it.
// Only one check runs at a time (the engine keeps a global log sink while checking); a request that arrives meanwhile
// makes one more run start right after, so the result always reflects the newest saved state.
#pragma once
#include <QObject>
#include <QString>
#include <memory>
#include <vector>

#include "common.h"

class QThread;

namespace ote_native {

class ProblemsRunner : public QObject {
  Q_OBJECT
 public:
  explicit ProblemsRunner(QObject* parent = nullptr);
  ~ProblemsRunner() override;

  void run(const QString& folder);
  bool isRunning() const { return thread_ != nullptr; }
  const std::vector<ote_editor::Problem>& problems() const { return problems_; }
  // The folder the current problems() were computed for.
  const QString& checkedFolder() const { return checkedFolder_; }

 signals:
  void started();
  void finished();  // problems() is up to date (no further run is pending)

 private:
  void startThread();
  void onThreadDone();

  struct Shared {
    QString folder;
    std::vector<ote_editor::Problem> result;
  };
  QThread* thread_ = nullptr;
  std::shared_ptr<Shared> shared_;
  QString pendingFolder_;
  bool rerun_ = false;
  QString checkedFolder_;
  std::vector<ote_editor::Problem> problems_;
};

}  // namespace ote_native
