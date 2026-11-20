// One JSON data file of a project, held as an ote::Json tree. Key order and keys the editor does not know survive,
// because the tree is edited in place and written back as it is. All changes go through JsonCommands (json_commands.h);
// the apply* functions here are what those commands call, and they announce every change twice (before and after) so
// item models can use Qt's begin/end protocol.
#pragma once
#include <QByteArray>
#include <QObject>
#include <QString>

#include "core/base/json.h"
#include "json_path.h"

namespace ote_native {

// What changed. Set: `path` is the node that was replaced. Insert/Erase: `path` is the container, `index` the position
// (and `key` the member name for objects). Move: `path` is the container, a child moved from `index` to `to`.
struct JsonChange {
  enum class Kind { Set, Insert, Erase, Move };
  Kind kind = Kind::Set;
  JsonPath path;
  size_t index = 0;
  size_t to = 0;
  std::string key;
};

class JsonDocument : public QObject {
  Q_OBJECT
 public:
  JsonDocument(QString relPath, QString absPath);

  const QString& relPath() const { return relPath_; }
  const QString& absPath() const { return absPath_; }
  const ote::Json& root() const { return root_; }

  // Reads and parses the file again, dropping every unsaved edit. Returns false (with `error`) when the file cannot
  // be read or is not valid JSON; the previous tree is kept then.
  bool load(QString* error);

  // The text save() writes: two-space indented, trailing newline.
  QByteArray serialize() const;

  // The file contained // or /* */ comments when it was read; saving drops them. `commentsAcknowledged` is set by the UI
  // once the user agreed, so the warning appears once, before the first save.
  bool hasComments() const { return hasComments_; }
  bool commentsAcknowledged() const { return commentsAcknowledged_; }
  void setCommentsAcknowledged(bool v) { commentsAcknowledged_ = v; }

  bool isDirty() const { return version_ != savedVersion_; }
  // The file on disk no longer matches what was loaded or last saved (set by the project's file watcher).
  bool externallyChanged() const { return externallyChanged_; }

  // Bumped by load(): commands recorded for an older tree turn into no-ops instead of touching the wrong nodes.
  int generation() const { return generation_; }

  // --- used by the commands (undo and redo call these) ---
  bool applySet(const JsonPath& path, const ote::Json& value);
  bool applyInsert(const JsonPath& container, size_t index, const std::string& key, const ote::Json& value);
  bool applyErase(const JsonPath& container, size_t index);
  bool applyMove(const JsonPath& container, size_t from, size_t to);
  void bumpVersion(int delta);
  // Called when a fresh command is pushed while the saved state lies in the undone future: it can never be reached again.
  void forgetUnreachableSavedState();

  // --- used by Project ---
  void markSaved(const QByteArray& diskBytes);
  void setExternallyChanged(bool v);
  const QByteArray& diskHash() const { return diskHash_; }
  static QByteArray hashOf(const QByteArray& bytes);

 signals:
  void aboutToChange(const JsonChange& change);
  void changed(const JsonChange& change);
  void stateChanged();  // dirty / externally-changed flags
  void reloaded();      // the whole tree was replaced (load); models start from scratch

 private:
  QString relPath_, absPath_;
  ote::Json root_;
  bool hasComments_ = false, commentsAcknowledged_ = false, externallyChanged_ = false;
  int version_ = 0, savedVersion_ = 0, generation_ = 0;
  QByteArray diskHash_;
};

}  // namespace ote_native
