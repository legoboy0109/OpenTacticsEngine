// A project is a game folder. It owns the documents (one per JSON data file, loaded on first use), the single undo stack
// shared by all of them, saving with conflict detection, and a watcher that notices files changed behind the editor's back.
#pragma once
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QUndoStack>

#include "json_document.h"

namespace ote_native {

struct SaveResult {
  enum class Status {
    Saved,
    Unchanged,  // nothing to write: the document has no unsaved edits
    Conflict,   // the file on disk is not what was loaded: nothing was written, the UI decides (reload / keep mine)
    Failed
  };
  Status status = Status::Saved;
  QString message;
};

class Project : public QObject {
  Q_OBJECT
 public:
  explicit Project(const QString& folder, QObject* parent = nullptr);

  const QString& folder() const { return folder_; }
  // Project-relative paths ('/' separators) of every .json file, sorted.
  const QStringList& jsonFiles() const { return files_; }
  QString absolutePath(const QString& rel) const;

  // The document for `rel`, loaded on first request; nullptr (and `error`) when it cannot be read or parsed.
  JsonDocument* document(const QString& rel, QString* error = nullptr);
  JsonDocument* loadedDocument(const QString& rel) const { return docs_.value(rel, nullptr); }

  QUndoStack* undoStack() { return &stack_; }
  // Pushes an edit command of `doc` on the shared stack (keeps dirty tracking right when the saved state was undone away).
  void push(JsonDocument* doc, QUndoCommand* cmd);

  // Writes the document atomically (temp file + rename). A file that changed on disk since it was loaded or last saved is
  // not overwritten unless `overwrite` is set; the result says Conflict instead.
  SaveResult save(const QString& rel, bool overwrite = false);
  // Drops unsaved edits and reads the file again.
  bool reload(const QString& rel, QString* error = nullptr);

  QStringList dirtyFiles() const;
  bool hasDirtyFiles() const { return !dirtyFiles().isEmpty(); }

  // Compares the file on disk with what the document last saw, and flags it when different. The watcher calls this;
  // tests call it directly to avoid waiting for file-system events.
  void checkExternalChange(const QString& rel);
  void rescanFiles();

 signals:
  void fileListChanged();
  void fileStateChanged(const QString& rel);  // dirty / externally-changed flag of that file changed
  void saved(const QString& rel);
  void documentReloaded(const QString& rel);

 private:
  void watchAll();
  void flushWatcherEvents();

  QString folder_;
  QStringList files_;
  QHash<QString, JsonDocument*> docs_;
  QUndoStack stack_;
  QFileSystemWatcher watcher_;
  QTimer debounce_;
  QSet<QString> pendingPaths_;
};

}  // namespace ote_native
