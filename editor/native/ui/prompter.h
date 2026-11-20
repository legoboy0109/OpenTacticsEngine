// Every question the editor can ask the user, behind one interface: the real one shows message boxes, tests install a
// scripted one (so no modal dialog ever blocks a headless test run).
#pragma once
#include <QString>
#include <QStringList>

class QWidget;

namespace ote_native {

class Prompter {
 public:
  enum class ConflictChoice { Reload, KeepMine, Cancel };
  enum class SaveChoice { Save, Discard, Cancel };
  virtual ~Prompter() = default;

  // The file has comments that saving will drop. True = save anyway.
  virtual bool confirmCommentLoss(const QString& relPath) = 0;
  // The file on disk is not the one that was loaded. Reload throws the editor's changes away, KeepMine overwrites the disk.
  virtual ConflictChoice resolveConflict(const QString& relPath, const QString& message) = 0;
  // There are unsaved files and the user is about to leave (close, open another folder, play).
  virtual SaveChoice askSaveChanges(const QStringList& dirtyFiles, const QString& why) = 0;
  virtual void showError(const QString& title, const QString& message) = 0;
};

class MessageBoxPrompter : public Prompter {
 public:
  explicit MessageBoxPrompter(QWidget* parent) : parent_(parent) {}
  bool confirmCommentLoss(const QString& relPath) override;
  ConflictChoice resolveConflict(const QString& relPath, const QString& message) override;
  SaveChoice askSaveChanges(const QStringList& dirtyFiles, const QString& why) override;
  void showError(const QString& title, const QString& message) override;

 private:
  QWidget* parent_;
};

}  // namespace ote_native
