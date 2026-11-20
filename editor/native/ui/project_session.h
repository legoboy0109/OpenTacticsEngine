// An open project plus everything the window does with it that is not drawing: the save flow (comment warning,
// conflict question, problems check afterwards) and the "unsaved changes?" question before leaving.
#pragma once
#include <QObject>

#include "problems_runner.h"
#include "project.h"
#include "prompter.h"

namespace ote_native {

class ProjectSession : public QObject {
  Q_OBJECT
 public:
  ProjectSession(const QString& folder, Prompter* prompter, QObject* parent = nullptr);

  Project* project() { return &project_; }
  ProblemsRunner* problems() { return &problems_; }
  void setPrompter(Prompter* p) { prompter_ = p; }

  // Saves one file with the full flow. False when the user cancelled or the write failed.
  bool save(const QString& rel);
  bool saveAll();
  // Runs the engine check on what is on disk now.
  void checkProblems();
  // True when it is fine to leave the project (nothing unsaved, or the user chose Save/Don't save).
  bool confirmLeave(const QString& why);

 signals:
  void statusMessage(const QString& text);

 private:
  Project project_;
  ProblemsRunner problems_;
  Prompter* prompter_;
};

}  // namespace ote_native
