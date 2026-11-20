// A Prompter whose answers the test sets in advance; it records every question, so a test can also check that a
// question was (or was not) asked. No dialog is ever shown.
#pragma once
#include "prompter.h"

namespace ote_test {

class ScriptedPrompter : public ote_native::Prompter {
 public:
  bool commentAnswer = true;
  ConflictChoice conflictAnswer = ConflictChoice::Cancel;
  SaveChoice saveAnswer = SaveChoice::Cancel;
  QStringList commentAsked, conflictAsked, saveAsked, errors;

  bool confirmCommentLoss(const QString& rel) override { commentAsked << rel; return commentAnswer; }
  ConflictChoice resolveConflict(const QString& rel, const QString&) override { conflictAsked << rel; return conflictAnswer; }
  SaveChoice askSaveChanges(const QStringList& files, const QString&) override { saveAsked << files.join(","); return saveAnswer; }
  void showError(const QString& title, const QString& message) override { errors << title + ": " + message; }
};

}  // namespace ote_test
